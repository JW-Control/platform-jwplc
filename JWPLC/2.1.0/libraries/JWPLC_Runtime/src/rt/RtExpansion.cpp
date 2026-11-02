#include "RtExpansion.h"

#include <JWPLC_ExpansionProtocol.h>
#include <JWPLC_ModbusRTU.h>
#include <string.h>

namespace jwrt
{

namespace
{
const uint8_t MASTER_ID = 247;        // ID propio del puerto (el maestro no atiende pedidos)
const uint32_t RETRY_MS = 1000;       // sondeo de los módulos caídos
const uint8_t FAILS_OFFLINE = 3;

uint32_t serialConfig(uint8_t frame)
{
    return frame == 1 ? SERIAL_8E1 : (frame == 2 ? SERIAL_8O1 : SERIAL_8N1);
}

// Plazo de respuesta: 16 registros a 9600 baudios tardan unos 40 ms.
uint32_t timeoutMs(uint8_t baud)
{
    return baud == 0 ? 120 : (baud == 1 ? 80 : 50);
}

void parseIdent(const uint16_t ir[16], XInfo &info)
{
    info.product = ir[JWX_IR_PRODUCT - JWX_IR_FIRST];
    info.firmware = ir[JWX_IR_FIRMWARE - JWX_IR_FIRST];
    info.io = ir[JWX_IR_IO - JWX_IR_FIRST];
    info.caps = ir[JWX_IR_CAPS - JWX_IR_FIRST];
    info.status = (uint8_t)(ir[JWX_IR_STATUS - JWX_IR_FIRST] >> 8);
    for (uint8_t i = 0; i < 3; i++)
    {
        const uint16_t w = ir[JWX_IR_MAC01 - JWX_IR_FIRST + i];
        info.mac[2 * i] = (uint8_t)(w >> 8);
        info.mac[2 * i + 1] = (uint8_t)w;
    }
    info.uid = ((uint32_t)ir[JWX_IR_UID_HI - JWX_IR_FIRST] << 16) | ir[JWX_IR_UID_LO - JWX_IR_FIRST];
}

bool identOk(const uint16_t ir[16])
{
    return ir[0] == JWX_MAGIC1 && ir[1] == JWX_MAGIC2 && ir[JWX_IR_PROTOCOL - JWX_IR_FIRST] >= JWX_PROTOCOL_VERSION;
}
} // namespace

void Expansion::begin(Store &store)
{
    _store = &store;
    store.loadLinks(_links);
    const Config &c = store.config();
    _baud = c.busBaud >= 1 && c.busBaud <= JWX_BAUD_CODES ? (uint8_t)(c.busBaud - 1) : JWX_FACTORY_BAUD_CODE;
    _frame = c.busFrame < JWX_FRAME_CODES ? c.busFrame : JWX_FACTORY_FRAME_CODE;
    const uint64_t mac = ESP.getEfuseMac();
    uint8_t m[6];
    for (uint8_t i = 0; i < 6; i++)
    {
        m[i] = (uint8_t)(mac >> (8 * i));
    }
    _masterUid = crc32(m, 6);
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        _state[x] = used(x) ? XS_CHECK : XS_NONE;
    }
    if (count())
    {
        startBus(_baud, _frame);
    }
}

void Expansion::startBus(uint8_t baud, uint8_t frame)
{
    JWPLC_ModbusRTU.end();
    JWPLC_ModbusRTU.begin(MASTER_ID, JWX_BAUDS[baud], serialConfig(frame));
    _busOn = true;
    _pending = false;
}

void Expansion::waitIdle()
{
    const uint32_t t0 = millis();
    while (JWPLC_ModbusRTU.masterBusy() && millis() - t0 < 300)
    {
        JWPLC_ModbusRTU.task();
        vTaskDelay(1);
    }
    JWPLC_ModbusRTU.clearMasterResult();
    _pending = false;
}

uint8_t Expansion::count() const
{
    uint8_t n = 0;
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        n += used(x) ? 1 : 0;
    }
    return n;
}

void Expansion::setState(uint8_t module, uint8_t state)
{
    if (_state[module] != state)
    {
        // CHECK es transitorio: no genera evento.
        if (state != XS_CHECK)
        {
            _changed |= (uint8_t)(1u << module);
        }
        _state[module] = state;
    }
}

bool Expansion::takeChange(uint8_t &module, uint8_t &state)
{
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        if (_changed & (1u << x))
        {
            _changed &= (uint8_t)~(1u << x);
            module = x;
            state = _state[x];
            return true;
        }
    }
    return false;
}

void Expansion::errCode(char out[], size_t size) const
{
    out[0] = 0;
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        if (_state[x] == XS_OFFLINE)
        {
            snprintf(out, size, "X%u", x + 1);
            return;
        }
        if (_state[x] == XS_WRONG)
        {
            snprintf(out, size, "XU%u", x + 1);
            return;
        }
    }
}

void Expansion::verify(uint8_t module, const XInfo &info)
{
    if (info.uid != _links[module].uid)
    {
        setState(module, XS_WRONG);
        _retryAt[module] = millis() + RETRY_MS;
        return;
    }
    _failRun[module] = 0;
    setState(module, XS_ONLINE);
}

void Expansion::service(uint32_t nowMs, bool run)
{
    if (!_busOn || !count())
    {
        return;
    }
    JWPLC_ModbusRTU.task();
    const uint8_t x = _cur;
    if (_pending)
    {
        if (JWPLC_ModbusRTU.masterBusy())
        {
            return;
        }
        const bool ok = JWPLC_ModbusRTU.masterSucceeded();
        JWPLC_ModbusRTU.clearMasterResult();
        _pending = false;
        if (!ok)
        {
            _failTotal[x]++;
            if (_state[x] == XS_ONLINE && ++_failRun[x] < FAILS_OFFLINE)
            {
                // Se reintenta en la próxima vuelta.
            }
            else if (_state[x] != XS_WRONG)
            {
                setState(x, XS_OFFLINE);
                _in[x] = 0;
                _retryAt[x] = nowMs + RETRY_MS;
            }
            _step = 3; // siguiente módulo
        }
        else if (_step == 0)
        {
            XInfo info;
            memset(&info, 0, sizeof(info));
            if (identOk(_rx))
            {
                parseIdent(_rx, info);
                verify(x, info);
            }
            else
            {
                setState(x, XS_WRONG);
                _retryAt[x] = nowMs + RETRY_MS;
            }
            _step = _state[x] == XS_ONLINE ? 1 : 3;
        }
        else if (_step == 1)
        {
            _failRun[x] = 0;
            _step = 2;
        }
        else
        {
            _in[x] = _rxBits;
            _failRun[x] = 0;
            _step = 3;
        }
    }

    // Próximo intercambio.
    for (uint8_t guard = 0; guard < NUM_X + 1; guard++)
    {
        if (_step >= 3 || !used(_cur))
        {
            _cur = (uint8_t)((_cur + 1) % NUM_X);
            _step = 0;
            if (_cur == 0)
            {
                _cycleMs = nowMs - _cycleStart;
                _cycleStart = nowMs;
            }
            continue;
        }
        const uint8_t m = _cur;
        const uint8_t id = _links[m].id;
        const uint32_t to = timeoutMs(_baud);
        if (_step == 0)
        {
            if (_state[m] == XS_ONLINE)
            {
                _step = 1;
            }
            else if (_state[m] != XS_CHECK && (int32_t)(nowMs - _retryAt[m]) < 0)
            {
                _step = 3;
                continue;
            }
            else
            {
                _pending = JWPLC_ModbusRTU.requestReadInputRegisters(id, JWX_IR_FIRST, JWX_IR_COUNT, _rx, to);
                return;
            }
        }
        if (_step == 1)
        {
            static uint8_t bits;
            bits = run ? _out[m] : 0;
            _pending = JWPLC_ModbusRTU.requestWriteMultipleCoils(id, 0, X_IO, &bits, to);
            return;
        }
        _pending = JWPLC_ModbusRTU.requestReadDiscreteInputs(id, 0, X_IO, &_rxBits, to);
        return;
    }
}

bool Expansion::setBus(uint8_t baud, uint8_t frame)
{
    if (count() || baud >= JWX_BAUD_CODES || frame >= JWX_FRAME_CODES)
    {
        return false; // con módulos vinculados el bus no se cambia
    }
    _baud = baud;
    _frame = frame;
    Config &c = _store->config();
    c.busBaud = (uint8_t)(baud + 1);
    c.busFrame = frame;
    return _store->saveConfig();
}

bool Expansion::probe(uint8_t id, bool factoryFrame, XInfo &info)
{
    memset(&info, 0, sizeof(info));
    info.id = id;
    waitIdle();
    if (factoryFrame)
    {
        startBus(JWX_FACTORY_BAUD_CODE, JWX_FACTORY_FRAME_CODE);
    }
    else if (!_busOn || JWPLC_ModbusRTU.baudRate() != JWX_BAUDS[_baud])
    {
        startBus(_baud, _frame);
    }
    uint16_t ir[16];
    uint16_t hr[JWX_HR_COUNT];
    const uint32_t to = timeoutMs(factoryFrame ? JWX_FACTORY_BAUD_CODE : _baud);
    bool ok = JWPLC_ModbusRTU.readInputRegistersSync(id, JWX_IR_FIRST, JWX_IR_COUNT, ir, to) && identOk(ir);
    if (ok)
    {
        parseIdent(ir, info);
        ok = JWPLC_ModbusRTU.readHoldingRegistersSync(id, JWX_HR_FIRST, JWX_HR_COUNT, hr, to);
    }
    if (ok)
    {
        info.baud = (uint8_t)hr[JWX_HR_BAUD - JWX_HR_FIRST];
        info.frame = (uint8_t)hr[JWX_HR_FRAME - JWX_HR_FIRST];
        info.safeValue = (uint8_t)hr[JWX_HR_SAFE_VALUE - JWX_HR_FIRST];
        info.safeHold = (uint8_t)hr[JWX_HR_SAFE_HOLD - JWX_HR_FIRST];
        info.watchdog = (uint8_t)hr[JWX_HR_WATCHDOG - JWX_HR_FIRST];
        info.flags = (uint8_t)hr[JWX_HR_FLAGS - JWX_HR_FIRST];
        info.link = ((uint32_t)hr[JWX_HR_LINK_HI - JWX_HR_FIRST] << 16) | hr[JWX_HR_LINK_LO - JWX_HR_FIRST];
    }
    info.ok = ok;
    if (factoryFrame)
    {
        startBus(_baud, _frame);
    }
    lastProbe = info;
    return ok;
}

// Escribe 224..236 con el comando 5 y espera el resultado.
bool Expansion::writeConfig(uint8_t id, const XInfo &cfg, char why[], size_t size)
{
    uint16_t hr[JWX_HR_COUNT] = {JWX_UNLOCK_KEY, cfg.id, cfg.baud, JWX_CMD_SAVE_ALL, 0, JWX_RES_BUSY, cfg.frame,
                                 cfg.safeValue, cfg.safeHold, cfg.watchdog, 0,
                                 (uint16_t)(_masterUid >> 16), (uint16_t)_masterUid};
    if (!JWPLC_ModbusRTU.writeMultipleRegistersSync(id, JWX_HR_FIRST, JWX_HR_COUNT, hr, 200))
    {
        snprintf(why, size, "Sin respuesta en ID %u", id);
        return false;
    }
    // La respuesta salió antes de procesar el comando: se lee el resultado
    // (en el ID viejo, durante los 50 ms antes de que el esclavo cambie).
    vTaskDelay(pdMS_TO_TICKS(10));
    uint16_t res = JWX_RES_BUSY;
    JWPLC_ModbusRTU.readHoldingRegistersSync(id, JWX_HR_RESULT, 1, &res, 100);
    if (res != JWX_RES_OK && res != JWX_RES_BUSY)
    {
        snprintf(why, size, res == JWX_RES_LOCKED ? "Configurado por su sketch"
                            : (res == JWX_RES_RANGE ? "Valor fuera de rango" : "El modulo no guardo"));
        return false;
    }
    return true;
}

uint8_t Expansion::freeId() const
{
    for (uint8_t id = 1; id <= JWX_MAX_ID; id++)
    {
        bool taken = false;
        for (uint8_t x = 0; x < NUM_X; x++)
        {
            taken = taken || (used(x) && _links[x].id == id);
        }
        if (!taken)
        {
            return id;
        }
    }
    return 0;
}

static int8_t freeSlot(const XLink links[NUM_X])
{
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        if (!(links[x].flags & XL_USED))
        {
            return (int8_t)x;
        }
    }
    return -1;
}

static void fillLink(XLink &l, const XInfo &info)
{
    memset(&l, 0, sizeof(l));
    l.flags = (uint8_t)(XL_USED | (info.status == JWX_STATUS_LOCKED ? XL_LOCKED : 0));
    l.id = info.id;
    l.product = (uint8_t)info.product;
    l.io = (uint8_t)(((info.io >> 8) & 0x0F) << 4 | (info.io & 0x0F));
    l.safeValue = info.safeValue;
    l.safeHold = info.safeHold;
    l.watchdog = info.watchdog;
    l.firmware = (uint8_t)(((info.firmware >> 8) & 0x0F) << 4 | (info.firmware & 0x0F));
    l.uid = info.uid;
}

int8_t Expansion::addFactory(uint8_t newId, uint8_t safeValue, uint8_t safeHold, uint8_t watchdog,
                             char why[], size_t size)
{
    const int8_t slot = freeSlot(_links);
    if (slot < 0)
    {
        snprintf(why, size, "Ya hay %u modulos", NUM_X);
        return -1;
    }
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        if (used(x) && _links[x].id == newId)
        {
            snprintf(why, size, "El ID %u ya es del modulo %u", newId, x + 1);
            return -1;
        }
    }
    XInfo f;
    if (!probe(JWX_FACTORY_ID, true, f))
    {
        snprintf(why, size, "Sin modulo en el ID 247");
        return -1;
    }
    XInfo cfg = f;
    cfg.id = newId;
    cfg.baud = _baud;
    cfg.frame = _frame;
    cfg.safeValue = safeValue;
    cfg.safeHold = safeHold;
    cfg.watchdog = watchdog;
    startBus(JWX_FACTORY_BAUD_CODE, JWX_FACTORY_FRAME_CODE);
    const bool wrote = writeConfig(JWX_FACTORY_ID, cfg, why, size);
    startBus(_baud, _frame);
    if (!wrote)
    {
        return -1;
    }
    vTaskDelay(pdMS_TO_TICKS(150));
    XInfo n;
    if (!probe(newId, false, n) || n.uid != f.uid || n.link != _masterUid)
    {
        snprintf(why, size, "No responde en el ID %u", newId);
        return -1;
    }
    fillLink(_links[slot], n);
    _state[slot] = XS_ONLINE;
    _failRun[slot] = 0;
    _failTotal[slot] = 0;
    if (!_store->saveLinks(_links))
    {
        snprintf(why, size, "No se pudo guardar en FRAM");
    }
    return slot;
}

int8_t Expansion::linkExisting(uint8_t id, char why[], size_t size)
{
    const int8_t slot = freeSlot(_links);
    if (slot < 0)
    {
        snprintf(why, size, "Ya hay %u modulos", NUM_X);
        return -1;
    }
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        if (used(x) && _links[x].id == id)
        {
            snprintf(why, size, "Ya es el modulo %u", x + 1);
            return -1;
        }
    }
    XInfo info;
    if (!probe(id, false, info))
    {
        snprintf(why, size, "Sin respuesta en ID %u", id);
        return -1;
    }
    if (info.status == JWX_STATUS_FACTORY)
    {
        snprintf(why, size, "Esta de fabrica: use Agregar");
        return -1;
    }
    if (info.status != JWX_STATUS_LOCKED && info.link != _masterUid)
    {
        // Se queda con su configuración; solo se anota el vínculo.
        if (!writeConfig(id, info, why, size))
        {
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(150));
        probe(id, false, info);
    }
    fillLink(_links[slot], info);
    _state[slot] = XS_ONLINE;
    _failRun[slot] = 0;
    _failTotal[slot] = 0;
    _store->saveLinks(_links);
    if (!_busOn)
    {
        startBus(_baud, _frame);
    }
    return slot;
}

bool Expansion::setSafe(uint8_t module, uint8_t safeValue, uint8_t safeHold, uint8_t watchdog, char why[],
                        size_t size)
{
    if (!used(module))
    {
        return false;
    }
    if (_links[module].flags & XL_LOCKED)
    {
        snprintf(why, size, "Configurado por su sketch");
        return false;
    }
    XInfo info;
    if (!probe(_links[module].id, false, info) || info.uid != _links[module].uid)
    {
        snprintf(why, size, "Sin respuesta en ID %u", _links[module].id);
        return false;
    }
    info.safeValue = safeValue;
    info.safeHold = safeHold;
    info.watchdog = watchdog;
    if (!writeConfig(info.id, info, why, size))
    {
        return false;
    }
    _links[module].safeValue = safeValue;
    _links[module].safeHold = safeHold;
    _links[module].watchdog = watchdog;
    vTaskDelay(pdMS_TO_TICKS(100));
    return _store->saveLinks(_links);
}

bool Expansion::remove(uint8_t module)
{
    if (!used(module))
    {
        return false;
    }
    memset(&_links[module], 0, sizeof(XLink));
    _state[module] = XS_NONE;
    _in[module] = 0;
    _changed &= (uint8_t)~(1u << module);
    return _store->saveLinks(_links);
}

uint8_t Expansion::scan(XFound found[], uint8_t max)
{
    waitIdle();
    startBus(_baud, _frame);
    uint8_t n = 0;
    uint16_t ir[16];
    for (uint8_t id = 1; id <= JWX_FACTORY_ID && n < max; id++)
    {
        if (!JWPLC_ModbusRTU.readInputRegistersSync(id, JWX_IR_FIRST, JWX_IR_COUNT, ir, 20) || !identOk(ir))
        {
            continue;
        }
        XFound &f = found[n++];
        f.id = id;
        f.status = (uint8_t)(ir[JWX_IR_STATUS - JWX_IR_FIRST] >> 8);
        f.module = -1;
        const uint32_t uid = ((uint32_t)ir[JWX_IR_UID_HI - JWX_IR_FIRST] << 16) | ir[JWX_IR_UID_LO - JWX_IR_FIRST];
        for (uint8_t x = 0; x < NUM_X; x++)
        {
            if (used(x) && _links[x].id == id && _links[x].uid == uid)
            {
                f.module = (int8_t)x;
            }
        }
        uint16_t link[2] = {0, 0};
        JWPLC_ModbusRTU.readHoldingRegistersSync(id, JWX_HR_LINK_HI, 2, link, 50);
        const uint32_t l = ((uint32_t)link[0] << 16) | link[1];
        f.linked = l == 0 ? 0 : (l == _masterUid ? 1 : 2);
    }
    if (!count())
    {
        // Sin módulos, el puerto queda libre.
        JWPLC_ModbusRTU.end();
        _busOn = false;
    }
    return n;
}

} // namespace jwrt
