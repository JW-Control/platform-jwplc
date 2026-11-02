// Esclavo de expansión: el JWPLC Basic como módulo DI8/DO8 de otro JWPLC.
//
// Corre en su propia tarea: lee entradas, atiende Modbus RTU, procesa los
// comandos de configuración (registros 224..236), aplica el estado seguro si
// el maestro deja de escribir las salidas y muestra su estado en la pantalla
// RUN (título "Esclavo ID n", LED RUN = maestro presente, ERR "FS" = estado
// seguro aplicado).
#include "JWPLC_Expansion.h"

#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_IdleScreen.h>
#include <JWPLC_ModbusRTU.h>
#include <string.h>

extern "C"
{
#include "jwplc_peripherals.h"
}

JWPLC_ExpansionSlaveClass JWPLC_ExpansionSlave;

namespace
{

// FRAM: después del área del Runtime (0x0000..0x117F) y antes del último KiB
// del sistema. Un equipo puede pasar de maestro a esclavo sin pisar nada.
const uint32_t ADDR_SLAVE_CFG = 0x1180;
const uint32_t CFG_MAGIC = 0x3258574AUL; // "JWX2"

struct SlaveConfig
{
    uint32_t magic;
    uint8_t id;
    uint8_t baud;      // código 0..4
    uint8_t frame;     // código 0..2
    uint8_t watchdog;  // x 100 ms
    uint8_t safeValue; // bit n = Q(n+1) encendida en estado seguro
    uint8_t safeHold;  // bit n = Q(n+1) conserva su valor
    uint8_t flags;     // JWX_FLAG_*
    uint8_t reserved;
    uint32_t link;     // CRC del UID del maestro (0 = sin vínculo)
    uint32_t crc;
};

SlaveConfig g_cfg;
bool g_locked = false;
bool g_fram = false;
uint16_t g_hr[256];
uint16_t g_ir[256];
uint8_t g_coils = 0;
uint8_t g_di = 0;
uint8_t g_applied = 0;
bool g_safe = false;
bool g_masterSeen = false;
uint32_t g_applyAt = 0;   // 0 = nada pendiente; si no, millis() en que se aplica la nueva trama
bool g_rebootPending = false;
TaskHandle_t g_task = nullptr;
char g_title[24];

uint32_t crc32(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFUL;
    while (n--)
    {
        c ^= *d++;
        for (uint8_t k = 0; k < 8; k++)
        {
            c = (c >> 1) ^ (0xEDB88320UL & (0UL - (c & 1)));
        }
    }
    return ~c;
}

uint32_t cfgCrc(const SlaveConfig &c)
{
    return crc32((const uint8_t *)&c, sizeof(c) - sizeof(c.crc));
}

void factory(SlaveConfig &c)
{
    memset(&c, 0, sizeof(c));
    c.magic = CFG_MAGIC;
    c.id = JWX_FACTORY_ID;
    c.baud = JWX_FACTORY_BAUD_CODE;
    c.frame = JWX_FACTORY_FRAME_CODE;
    c.watchdog = JWX_FACTORY_WATCHDOG;
}

bool saveConfig()
{
    g_cfg.crc = cfgCrc(g_cfg);
    // JW_FRAM::write() no activa WREN por sí solo.
    if (!JWPLC_FRAM.writeEnable(true))
    {
        return false;
    }
    const bool ok = JWPLC_FRAM.write(ADDR_SLAVE_CFG, (const uint8_t *)&g_cfg, sizeof(g_cfg));
    JWPLC_FRAM.writeEnable(false);
    SlaveConfig check;
    return ok && JWPLC_FRAM.read(ADDR_SLAVE_CFG, (uint8_t *)&check, sizeof(check)) &&
           memcmp(&check, &g_cfg, sizeof(check)) == 0;
}

bool loadConfig()
{
    SlaveConfig c;
    if (!JWPLC_FRAM.read(ADDR_SLAVE_CFG, (uint8_t *)&c, sizeof(c)))
    {
        return false;
    }
    if (c.magic != CFG_MAGIC || c.crc != cfgCrc(c) || c.id < 1 || c.id > JWX_FACTORY_ID ||
        c.baud >= JWX_BAUD_CODES || c.frame >= JWX_FRAME_CODES)
    {
        return false;
    }
    g_cfg = c;
    return true;
}

uint32_t serialConfig(uint8_t frame)
{
    return frame == 1 ? SERIAL_8E1 : (frame == 2 ? SERIAL_8O1 : SERIAL_8N1);
}

uint8_t frameCode(uint32_t config)
{
    return config == SERIAL_8E1 ? 1 : (config == SERIAL_8O1 ? 2 : 0);
}

int8_t baudCode(uint32_t baud)
{
    for (uint8_t i = 0; i < JWX_BAUD_CODES; i++)
    {
        if (JWX_BAUDS[i] == baud)
        {
            return (int8_t)i;
        }
    }
    return -1;
}

uint8_t statusCode()
{
    if (g_locked)
    {
        return JWX_STATUS_LOCKED;
    }
    return (g_cfg.flags & JWX_FLAG_CONFIGURED) ? JWX_STATUS_CONFIGURED : JWX_STATUS_FACTORY;
}

uint16_t flagsReg()
{
    return (uint16_t)((g_cfg.flags & (JWX_FLAG_CONFIGURED | JWX_FLAG_LINKED)) | (g_locked ? JWX_FLAG_LOCKED : 0));
}

// Registros de identificación (FC04) y espejo de la configuración (FC03).
void publish()
{
    const uint64_t mac = ESP.getEfuseMac();
    uint8_t m[6];
    for (uint8_t i = 0; i < 6; i++)
    {
        m[i] = (uint8_t)(mac >> (8 * i));
    }
    const uint32_t uid = crc32(m, 6);
    g_ir[JWX_IR_MAGIC1] = JWX_MAGIC1;
    g_ir[JWX_IR_MAGIC2] = JWX_MAGIC2;
    g_ir[JWX_IR_PROTOCOL] = JWX_PROTOCOL_VERSION;
    g_ir[JWX_IR_PRODUCT] = JWX_PRODUCT_BASIC;
    g_ir[JWX_IR_FIRMWARE] = 0x0100; // 1.0
    g_ir[JWX_IR_IO] = 0x0808;
    g_ir[JWX_IR_CAPS] = (uint16_t)(JWX_CAP_DIGITAL_IO | JWX_CAP_ID_BY_MODBUS | JWX_CAP_FRAM |
                                   JWX_CAP_DISPLAY | JWX_CAP_SAFE_STATE | (g_locked ? JWX_CAP_LOCKED : 0));
    g_ir[JWX_IR_STATUS] = (uint16_t)((statusCode() << 8) | g_cfg.id);
    g_ir[JWX_IR_MAC01] = (uint16_t)((m[0] << 8) | m[1]);
    g_ir[JWX_IR_MAC23] = (uint16_t)((m[2] << 8) | m[3]);
    g_ir[JWX_IR_MAC45] = (uint16_t)((m[4] << 8) | m[5]);
    g_ir[JWX_IR_UID_HI] = (uint16_t)(uid >> 16);
    g_ir[JWX_IR_UID_LO] = (uint16_t)uid;
    g_ir[JWX_IR_CFG_CRC] = (uint16_t)cfgCrc(g_cfg);

    g_hr[JWX_HR_KEY] = 0;
    g_hr[JWX_HR_ID] = g_cfg.id;
    g_hr[JWX_HR_BAUD] = g_cfg.baud;
    g_hr[JWX_HR_COMMAND] = 0;
    g_hr[JWX_HR_PARAM] = 0;
    g_hr[JWX_HR_FRAME] = g_cfg.frame;
    g_hr[JWX_HR_SAFE_VALUE] = g_cfg.safeValue;
    g_hr[JWX_HR_SAFE_HOLD] = g_cfg.safeHold;
    g_hr[JWX_HR_WATCHDOG] = g_cfg.watchdog;
    g_hr[JWX_HR_FLAGS] = flagsReg();
    g_hr[JWX_HR_LINK_HI] = (uint16_t)(g_cfg.link >> 16);
    g_hr[JWX_HR_LINK_LO] = (uint16_t)g_cfg.link;

    if (g_cfg.flags & JWX_FLAG_CONFIGURED || g_locked)
    {
        snprintf(g_title, sizeof(g_title), "Esclavo ID %u", g_cfg.id);
    }
    else
    {
        snprintf(g_title, sizeof(g_title), "Esclavo fabrica");
    }
    JWPLCIdleScreen::setTitle(g_title);
}

bool startBus()
{
    JWPLC_ModbusRTU.end();
    JWPLC_ModbusRTU.setCoils(&g_coils, 8);
    JWPLC_ModbusRTU.setDiscreteInputs(&g_di, 8);
    JWPLC_ModbusRTU.setHoldingRegisters(g_hr, 256);
    JWPLC_ModbusRTU.setInputRegisters(g_ir, 256);
    const bool ok = JWPLC_ModbusRTU.begin(g_cfg.id, JWX_BAUDS[g_cfg.baud], serialConfig(g_cfg.frame));
    // Mismo hueco que el esclavo Remote I/O validado en placa.
    JWPLC_ModbusRTU.setFrameGapMs(2);
    return ok;
}

uint16_t runCommand(uint16_t cmd)
{
    if (cmd == JWX_CMD_REBOOT)
    {
        g_rebootPending = true;
        g_applyAt = millis() + 50;
        return JWX_RES_OK;
    }
    if (g_hr[JWX_HR_KEY] != JWX_UNLOCK_KEY)
    {
        return JWX_RES_BAD_KEY;
    }
    if (g_locked)
    {
        return JWX_RES_LOCKED;
    }
    SlaveConfig next = g_cfg;
    switch (cmd)
    {
    case JWX_CMD_SAVE_ID:
        next.id = (uint8_t)g_hr[JWX_HR_ID];
        break;
    case JWX_CMD_SAVE_BAUD:
        next.baud = (uint8_t)g_hr[JWX_HR_BAUD];
        break;
    case JWX_CMD_FACTORY:
        factory(next);
        break;
    case JWX_CMD_SAVE_ALL:
        next.id = (uint8_t)g_hr[JWX_HR_ID];
        next.baud = (uint8_t)g_hr[JWX_HR_BAUD];
        next.frame = (uint8_t)g_hr[JWX_HR_FRAME];
        next.safeValue = (uint8_t)g_hr[JWX_HR_SAFE_VALUE];
        next.safeHold = (uint8_t)g_hr[JWX_HR_SAFE_HOLD];
        next.watchdog = (uint8_t)g_hr[JWX_HR_WATCHDOG];
        next.link = ((uint32_t)g_hr[JWX_HR_LINK_HI] << 16) | g_hr[JWX_HR_LINK_LO];
        break;
    default:
        return JWX_RES_RANGE;
    }
    if (cmd != JWX_CMD_FACTORY)
    {
        if (g_hr[JWX_HR_ID] > 0xFF || next.id < 1 || next.id > JWX_MAX_ID || next.baud >= JWX_BAUD_CODES ||
            next.frame >= JWX_FRAME_CODES || next.watchdog < JWX_MIN_WATCHDOG || next.watchdog > JWX_MAX_WATCHDOG)
        {
            return JWX_RES_RANGE;
        }
        next.flags = (uint8_t)(JWX_FLAG_CONFIGURED | (next.link ? JWX_FLAG_LINKED : 0));
    }
    const SlaveConfig old = g_cfg;
    g_cfg = next;
    if (!saveConfig())
    {
        g_cfg = old;
        return JWX_RES_FRAM;
    }
    // La respuesta a esta escritura ya salió con la trama anterior: el
    // cambio de ID y trama se aplica un momento después.
    g_applyAt = millis() + 50;
    return JWX_RES_OK;
}

void applyOutputs(uint8_t bits)
{
    if (bits != g_applied)
    {
        JWPLC_writeOutputs(bits);
        g_applied = bits;
    }
}

void serviceSafeState()
{
    const uint32_t limit = (uint32_t)g_cfg.watchdog * 100UL;
    const bool written = JWPLC_ModbusRTU.hasCoilWrite();
    const bool expired = !written || (uint32_t)(millis() - JWPLC_ModbusRTU.lastCoilWriteMs()) > limit;
    if (expired)
    {
        // Mantener = la salida conserva lo último aplicado; al arrancar, OFF.
        const uint8_t hold = written ? g_cfg.safeHold : 0;
        const uint8_t safe = (uint8_t)((g_applied & hold) | (g_cfg.safeValue & (uint8_t)~hold));
        applyOutputs(safe);
        g_coils = safe;
    }
    else
    {
        applyOutputs(g_coils);
    }
    if (expired != g_safe)
    {
        g_safe = expired;
        JWPLC_Display.setErrCode(expired && written ? "FS" : "");
    }
    const bool seen = JWPLC_ModbusRTU.hasValidRequest() &&
                      (uint32_t)(millis() - JWPLC_ModbusRTU.lastValidRequestMs()) <= limit;
    if (seen != g_masterSeen)
    {
        g_masterSeen = seen;
        JWPLC_Display.setRunLed(seen);
    }
}

void slaveTask(void *)
{
    for (;;)
    {
        g_di = JWPLC_readInputs();
        JWPLC_ModbusRTU.task();

        const uint16_t cmd = g_hr[JWX_HR_COMMAND];
        if (cmd != 0)
        {
            const uint16_t res = runCommand(cmd);
            const bool apply = res == JWX_RES_OK && g_applyAt != 0;
            if (!apply || g_rebootPending)
            {
                publish();
            }
            g_hr[JWX_HR_KEY] = 0;
            g_hr[JWX_HR_COMMAND] = 0;
            g_hr[JWX_HR_RESULT] = res;
            g_hr[JWX_HR_FLAGS] = flagsReg();
        }
        if (g_applyAt != 0 && (int32_t)(millis() - g_applyAt) >= 0)
        {
            g_applyAt = 0;
            if (g_rebootPending)
            {
                ESP.restart();
            }
            const uint16_t res = g_hr[JWX_HR_RESULT];
            publish();
            g_hr[JWX_HR_RESULT] = res;
            startBus();
        }
        serviceSafeState();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// ESC + OK mantenidas 3 s al encender: configuración de fábrica. Las dos
// teclas están en la fila 1 de la matriz (GPIO 2); la fila 0 (GPIO 12) es
// pin de arranque y no se usa para esto.
bool factoryKeysHeld()
{
    const uint32_t t0 = millis();
    while ((uint32_t)(millis() - t0) < 3000UL)
    {
        if (!JWPLC_Buttons.isDown(BTN_ESC) || !JWPLC_Buttons.isDown(BTN_OK))
        {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return true;
}

} // namespace

bool JWPLC_ExpansionSlaveClass::begin()
{
    g_fram = true;
    g_locked = false;
    if (!loadConfig())
    {
        factory(g_cfg);
    }
    if ((JWPLC_Buttons.isDown(BTN_ESC) && JWPLC_Buttons.isDown(BTN_OK)) && factoryKeysHeld())
    {
        factory(g_cfg);
        g_fram = saveConfig();
        JWPLC_Buttons.clearPendingInput();
    }
    return start(false);
}

bool JWPLC_ExpansionSlaveClass::begin(uint8_t id, uint32_t baud, uint32_t config)
{
    const int8_t code = baudCode(baud);
    if (id < 1 || id > JWX_MAX_ID || code < 0)
    {
        return false;
    }
    // El estado seguro y la vigilancia pueden venir de setSafeState()/setWatchdog().
    g_cfg.magic = CFG_MAGIC;
    g_cfg.id = id;
    g_cfg.baud = (uint8_t)code;
    g_cfg.frame = frameCode(config);
    if (g_cfg.watchdog < JWX_MIN_WATCHDOG)
    {
        g_cfg.watchdog = JWX_FACTORY_WATCHDOG;
    }
    g_cfg.flags = JWX_FLAG_CONFIGURED;
    g_locked = true;
    return start(true);
}

bool JWPLC_ExpansionSlaveClass::start(bool)
{
    g_coils = 0;
    g_applied = 0xFF;
    applyOutputs(0);
    g_di = JWPLC_readInputs();
    publish();
    g_hr[JWX_HR_RESULT] = JWX_RES_OK;
    // Sin maestro todavía: RUN apagado y ERR vacío.
    g_masterSeen = false;
    g_safe = false;
    JWPLC_Display.setRunLed(false);
    JWPLC_Display.setErrCode("");
    const bool ok = startBus();
    if (!g_task)
    {
        xTaskCreatePinnedToCore(slaveTask, "jwplc_expansion", 4096, nullptr, 2, &g_task, 1);
    }
    return ok;
}

void JWPLC_ExpansionSlaveClass::setSafeState(uint8_t q, uint8_t mode)
{
    if (q < 1 || q > 8)
    {
        return;
    }
    const uint8_t bit = (uint8_t)(1u << (q - 1));
    g_cfg.safeValue = (uint8_t)((g_cfg.safeValue & ~bit) | (mode == SAFE_ON ? bit : 0));
    g_cfg.safeHold = (uint8_t)((g_cfg.safeHold & ~bit) | (mode == SAFE_HOLD ? bit : 0));
}

void JWPLC_ExpansionSlaveClass::setWatchdog(uint16_t ms)
{
    uint16_t t = (uint16_t)(ms / 100);
    t = t < JWX_MIN_WATCHDOG ? JWX_MIN_WATCHDOG : (t > JWX_MAX_WATCHDOG ? JWX_MAX_WATCHDOG : t);
    g_cfg.watchdog = (uint8_t)t;
}

uint8_t JWPLC_ExpansionSlaveClass::id() const { return g_cfg.id; }
bool JWPLC_ExpansionSlaveClass::locked() const { return g_locked; }
bool JWPLC_ExpansionSlaveClass::linked() const { return (g_cfg.flags & JWX_FLAG_LINKED) != 0; }
bool JWPLC_ExpansionSlaveClass::safeActive() const { return g_safe; }
uint8_t JWPLC_ExpansionSlaveClass::outputs() const { return g_applied; }
