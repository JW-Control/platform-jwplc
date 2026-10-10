#include <stdlib.h>
#include <string.h>

extern "C"
{
#include "openplc.h"
}

#include "Arduino.h"
#include "vpp_config.h"
#include <JWPLC_ModbusRTU.h>

// ---------------------------------------------------------------------------
// JWPLC Basic como modulo Remote I/O del JWPLC Backplane (Modbus RTU esclavo).
//
// Mismo contrato que el sketch validado JWPLC_RemoteIO_Slave_RTU:
//   FC02 0..7  -> I0_0..I0_7
//   FC01 0..7  -> feedback Q0_0..Q0_7
//   FC05/FC15  -> Q0_0..Q0_7
//   FC03/FC06 HR 0 -> entradas forzadas en el Master (solo pantalla IDLE):
//              byte alto = mascara I0_0..I0_7, byte bajo = valor forzado.
//   Fail-safe: sin escritura FC05/FC15 valida durante el tiempo configurado
//   -> todas las Q en LOW y se borra la marca de entradas forzadas.
//
// Las salidas pertenecen al Master del Backplane: el programa IEC de este
// dispositivo no lee ni escribe I/O fisica (pinMapping = false), para que
// nunca haya dos programas mandando sobre el mismo rele.
//
// Configuracion desde la pantalla "Remote I/O Slave" (persistencia
// `remote_io_slave`), exportada por el Editor a vpp_config.h:
//   VPP_REMOTE_IO_SLAVE_SLAVE_ID       2
//   VPP_REMOTE_IO_SLAVE_BAUD_RATE      "115200"
//   VPP_REMOTE_IO_SLAVE_SERIAL_FORMAT  "8N1"
//   VPP_REMOTE_IO_SLAVE_FAILSAFE_MS    "1000"
// Sin definicion se usan los valores del sketch validado (ID 2, 115200/8N1,
// 1000 ms). Un valor no soportado detiene la compilacion.
// ---------------------------------------------------------------------------

// El runtime Baremetal referencia estas mascaras; este dispositivo no expone
// I/O fisica al programa IEC.
uint16_t pinMask_DIN[] = {0xFFFF};
uint16_t pinMask_AIN[] = {0xFFFF};
uint16_t pinMask_DOUT[] = {0xFFFF};
uint16_t pinMask_AOUT[] = {0xFFFF};

static constexpr uint8_t JWPLC_SLAVE_CHANNELS = 8;
static constexpr uint16_t JWPLC_SLAVE_FRAME_GAP_MS = 2;

static constexpr bool jwplcTextEquals(const char *a, const char *b)
{
    return (*a == *b) && (*a == '\0' || jwplcTextEquals(a + 1, b + 1));
}

static constexpr uint32_t jwplcSlaveBaud(const char *text)
{
    return jwplcTextEquals(text, "9600")     ? 9600UL
           : jwplcTextEquals(text, "19200")  ? 19200UL
           : jwplcTextEquals(text, "38400")  ? 38400UL
           : jwplcTextEquals(text, "57600")  ? 57600UL
           : jwplcTextEquals(text, "115200") ? 115200UL
                                             : 0UL;
}

static constexpr uint32_t jwplcSlaveSerialConfig(const char *text)
{
    return jwplcTextEquals(text, "8N1")   ? (uint32_t)SERIAL_8N1
           : jwplcTextEquals(text, "8E1") ? (uint32_t)SERIAL_8E1
           : jwplcTextEquals(text, "8O1") ? (uint32_t)SERIAL_8O1
                                          : 0UL;
}

// Failsafe: minimo 1000 ms. Debe superar el peor tiempo entre escrituras FC15
// del Master (ciclo de hasta 7 slots + un timeout de sondeo); valores menores
// hacen parpadear las salidas de modulos sanos.
static constexpr uint32_t jwplcSlaveFailsafeMs(const char *text)
{
    return jwplcTextEquals(text, "1000")   ? 1000UL
           : jwplcTextEquals(text, "2000") ? 2000UL
           : jwplcTextEquals(text, "5000") ? 5000UL
                                           : 0UL;
}

#if defined(VPP_REMOTE_IO_SLAVE_SLAVE_ID)
static constexpr double JWPLC_SLAVE_ID_RAW = (double)(VPP_REMOTE_IO_SLAVE_SLAVE_ID);
#else
static constexpr double JWPLC_SLAVE_ID_RAW = 2.0;
#endif

#if defined(VPP_REMOTE_IO_SLAVE_BAUD_RATE)
static constexpr uint32_t JWPLC_SLAVE_BAUD = jwplcSlaveBaud(VPP_REMOTE_IO_SLAVE_BAUD_RATE);
#else
static constexpr uint32_t JWPLC_SLAVE_BAUD = 115200UL;
#endif

#if defined(VPP_REMOTE_IO_SLAVE_SERIAL_FORMAT)
static constexpr uint32_t JWPLC_SLAVE_CONFIG = jwplcSlaveSerialConfig(VPP_REMOTE_IO_SLAVE_SERIAL_FORMAT);
#else
static constexpr uint32_t JWPLC_SLAVE_CONFIG = (uint32_t)SERIAL_8N1;
#endif

#if defined(VPP_REMOTE_IO_SLAVE_FAILSAFE_MS)
static constexpr uint32_t JWPLC_SLAVE_FAILSAFE_MS = jwplcSlaveFailsafeMs(VPP_REMOTE_IO_SLAVE_FAILSAFE_MS);
#else
static constexpr uint32_t JWPLC_SLAVE_FAILSAFE_MS = 1000UL;
#endif

static_assert(JWPLC_SLAVE_ID_RAW >= 1.0 && JWPLC_SLAVE_ID_RAW <= 247.0 &&
                  JWPLC_SLAVE_ID_RAW == (double)(long)JWPLC_SLAVE_ID_RAW,
              "JWPLC Remote I/O Slave: Slave ID debe ser un entero entre 1 y 247");
static_assert(JWPLC_SLAVE_BAUD != 0UL,
              "JWPLC Remote I/O Slave: baudrate no soportado (9600/19200/38400/57600/115200)");
static_assert(JWPLC_SLAVE_CONFIG != 0UL,
              "JWPLC Remote I/O Slave: formato serie no soportado (8N1/8E1/8O1)");
static_assert(JWPLC_SLAVE_FAILSAFE_MS != 0UL,
              "JWPLC Remote I/O Slave: failsafe no soportado (1000/2000/5000 ms)");

static constexpr uint8_t JWPLC_SLAVE_ID = (uint8_t)(long)JWPLC_SLAVE_ID_RAW;

static uint8_t jwplcSlaveCoils = 0x00;
static uint8_t jwplcSlaveDiscreteInputs = 0x00;
static uint8_t jwplcSlaveAppliedOutputs = 0xFF;
static bool jwplcSlaveEnabled = false;

// Holding Register 0: entradas de este modulo forzadas por el debugger en el
// Master (byte alto = mascara, byte bajo = valores). Solo alimenta la pantalla
// IDLE: el forzado actua en el programa del Master, no en la I/O de este equipo.
static constexpr uint16_t JWPLC_SLAVE_HR_INPUT_FORCE = 0;
static uint16_t jwplcSlaveHolding[1] = {0};

// Weak: con un package sin el overlay de la IDLE el HAL compila igual.
extern "C" void jwplcDisplaySetInputForceOverlay(uint8_t forcedMask, uint8_t forcedValues) __attribute__((weak));

static void jwplcSlavePublishForcedInputs()
{
    if (!jwplcDisplaySetInputForceOverlay)
        return;

    const uint16_t overlay = jwplcSlaveHolding[JWPLC_SLAVE_HR_INPUT_FORCE];
    // Barata si no cambio: el display solo se marca cuando el overlay varia.
    jwplcDisplaySetInputForceOverlay((uint8_t)(overlay >> 8), (uint8_t)(overlay & 0xFF));
}

static void jwplcSlaveApplyOutputs(uint8_t bitmap)
{
    if (bitmap == jwplcSlaveAppliedOutputs)
    {
        return;
    }

    JWPLC_writeOutputs(bitmap);
    jwplcSlaveAppliedOutputs = bitmap;
}

static void jwplcSlaveService()
{
    if (!jwplcSlaveEnabled)
    {
        return;
    }

    jwplcSlaveDiscreteInputs = JWPLC_readInputs();
    JWPLC_ModbusRTU.task();

    // Fail-safe: sin escritura valida del Master (nunca, o hace mas de
    // JWPLC_SLAVE_FAILSAFE_MS) todas las salidas quedan en LOW.
    if (!JWPLC_ModbusRTU.hasCoilWrite() ||
        (uint32_t)(millis() - JWPLC_ModbusRTU.lastCoilWriteMs()) > JWPLC_SLAVE_FAILSAFE_MS)
    {
        jwplcSlaveCoils = 0x00;
        jwplcSlaveApplyOutputs(0x00);
        // Sin Master no hay forzado vigente: no dejar una "F" vieja en la IDLE.
        jwplcSlaveHolding[JWPLC_SLAVE_HR_INPUT_FORCE] = 0;
        jwplcSlavePublishForcedInputs();
        return;
    }

    jwplcSlaveApplyOutputs(jwplcSlaveCoils);
    jwplcSlavePublishForcedInputs();
}

void hardwareInit()
{
    // La inicializacion de E/S industriales ya la realiza el core jwcontrol
    // mediante initPeripherals(), antes de que se ejecute setup().
    jwplcSlaveCoils = 0x00;
    jwplcSlaveDiscreteInputs = JWPLC_readInputs();
    jwplcSlaveApplyOutputs(0x00);

    JWPLC_ModbusRTU.setCoils(&jwplcSlaveCoils, JWPLC_SLAVE_CHANNELS);
    JWPLC_ModbusRTU.setDiscreteInputs(&jwplcSlaveDiscreteInputs, JWPLC_SLAVE_CHANNELS);
    JWPLC_ModbusRTU.setHoldingRegisters(jwplcSlaveHolding, 1);

    if (JWPLC_ModbusRTU.begin(JWPLC_SLAVE_ID, JWPLC_SLAVE_BAUD, JWPLC_SLAVE_CONFIG))
    {
        JWPLC_ModbusRTU.setFrameGapMs(JWPLC_SLAVE_FRAME_GAP_MS);
        jwplcSlaveEnabled = true;
    }
}

// El runtime lo llama continuamente en el tiempo libre del scan.
void hardwareService()
{
    jwplcSlaveService();
}

// Respaldo durante el scan: el esclavo tambien atiende el bus en cada ciclo.
void updateInputBuffers()
{
    jwplcSlaveService();
}

void updateOutputBuffers()
{
    jwplcSlaveService();
}
