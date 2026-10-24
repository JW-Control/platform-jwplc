#include <stdlib.h>
#include <string.h>

extern "C"
{
#include "openplc.h"
}

#include "Arduino.h"
#include "vpp_config.h"
#include <JWPLC_ModbusRTU.h>

// El Master usa motor(ASYNC) y la API unificada read...()/write...(), que
// llegan con el package Alpha12 (jwplc_modbus_motor.h).
#ifndef JWPLC_MODBUS_MOTOR_H
#error "JWPLC Backplane: requiere el package jwplc:esp32 2.1.0-alpha.12 o superior. Actualizar desde el Board Manager."
#endif

// Probe Alpha7.18: mide la cadencia del Backplane por Serial0.
// Alpha12: desactivado por defecto en produccion. Solo para banco de pruebas:
// definir JWPLC_ALPHA7_RTU_TIMING_DIAGNOSTICS=1 y mantener el Modbus RTU
// local/debugger de Serial0 apagado.
#ifndef JWPLC_ALPHA7_RTU_TIMING_DIAGNOSTICS
#define JWPLC_ALPHA7_RTU_TIMING_DIAGNOSTICS 0
#endif

// JWPLC Basic v2.x usa pines virtuales uint16_t:
// I0_0 = 0x2207, Q0_0 = 0x2208, etc.
// No usar uint8_t porque truncaria los pines.
//
// Para el VPP de JWPLC Basic el mapa físico es fijo:
//   I0_0..I0_7 -> entradas digitales
//   Q0_0..Q0_7 -> salidas digitales
//
// No depender de ../examples/Baremetal/defines.h porque OpenPLC 4.2.7
// no genera ese archivo dentro del flujo VPP.
uint16_t pinMask_DIN[] = {
    I0_0, I0_1, I0_2, I0_3,
    I0_4, I0_5, I0_6, I0_7
};

uint16_t pinMask_AIN[] = {
    0xFFFF
};

uint16_t pinMask_DOUT[] = {
    Q0_0, Q0_1, Q0_2, Q0_3,
    Q0_4, Q0_5, Q0_6, Q0_7
};

uint16_t pinMask_AOUT[] = {
    0xFFFF
};

static constexpr int NUM_DISCRETE_INPUT =
    sizeof(pinMask_DIN) / sizeof(pinMask_DIN[0]);

static constexpr int NUM_DISCRETE_OUTPUT =
    sizeof(pinMask_DOUT) / sizeof(pinMask_DOUT[0]);

static constexpr uint8_t JWPLC_REMOTE_CHANNELS = 8;
static constexpr uint8_t JWPLC_MODBUS_MASTER_LOCAL_ID = 247;
static constexpr uint32_t JWPLC_MODBUS_TIMEOUT_MS = 250UL;
static constexpr uint16_t JWPLC_MODBUS_FRAME_GAP_MS = 2;

// Slot 1 es el controlador fijo; los slots 2..8 alojan modulos Remote I/O.
static constexpr uint8_t JWPLC_REMOTE_MAX_SLOTS = 7;

// Fallos consecutivos (timeout/CRC/excepcion) antes de declarar un slot
// fuera de linea. Mientras no se alcance, las entradas conservan el ultimo
// valor valido; al alcanzarse pasan a 0 (estado seguro) y el slot solo se
// sondea con FC02 hasta que vuelva a responder.
static constexpr uint8_t JWPLC_REMOTE_OFFLINE_FAILURES = 3;

// Intervalo minimo entre sondeos a slots fuera de linea (uno por intervalo).
// Cada sondeo fallido cuesta un timeout completo; limitarlo acota el tiempo
// maximo entre escrituras FC15 a los modulos sanos, que deben mantenerse por
// debajo del failsafe de salidas del esclavo (JWPLC_RemoteIO_Slave_RTU:
// OUTPUT_FAILSAFE_MS = 1000). Peor caso: ciclo de slots en linea + 1 timeout.
static constexpr uint32_t JWPLC_REMOTE_OFFLINE_PROBE_INTERVAL_MS = 1000UL;

// Entradas remotas forzadas por el debugger -> pantalla IDLE del modulo.
// Se escriben con FC06 en el Holding Register 0 del esclavo (byte alto =
// mascara, byte bajo = valores) solo cuando cambian, mas un refresco mientras
// haya forzados (por si el modulo se reinicio). Un modulo sin ese registro
// (firmware anterior) responde excepcion: no cuenta como fallo de
// comunicacion y se reintenta como maximo una vez por intervalo.
static constexpr uint16_t JWPLC_REMOTE_HR_INPUT_FORCE = 0;
static constexpr uint32_t JWPLC_REMOTE_INPUT_FORCE_REFRESH_MS = 1000UL;

// ---------------------------------------------------------------------------
// Configuracion del bus RS-485 del Backplane (Serial2).
//
// La fuente de verdad es la seccion "RS-485 Backplane" de la pantalla
// JWPLC Backplane (persistencia `backplane_rtu`), que el Editor exporta a
// vpp_config.h como valores semanticos:
//   VPP_BACKPLANE_RTU_BAUD_RATE      "115200"
//   VPP_BACKPLANE_RTU_SERIAL_FORMAT  "8N1"
// Si el proyecto no los define (proyectos Alpha9 o campos nunca editados)
// se usa el perfil historico 115200 / 8N1. Un valor desconocido detiene la
// compilacion: nunca se cae silenciosamente a otro perfil de bus.
// ---------------------------------------------------------------------------
static constexpr bool jwplcTextEquals(const char *a, const char *b)
{
    return (*a == *b) && (*a == '\0' || jwplcTextEquals(a + 1, b + 1));
}

static constexpr uint32_t jwplcBackplaneBaud(const char *text)
{
    return jwplcTextEquals(text, "9600")     ? 9600UL
           : jwplcTextEquals(text, "19200")  ? 19200UL
           : jwplcTextEquals(text, "38400")  ? 38400UL
           : jwplcTextEquals(text, "57600")  ? 57600UL
           : jwplcTextEquals(text, "115200") ? 115200UL
                                             : 0UL;
}

static constexpr uint32_t jwplcBackplaneBaud(long value)
{
    return (value == 9600L || value == 19200L || value == 38400L ||
            value == 57600L || value == 115200L)
               ? (uint32_t)value
               : 0UL;
}

static constexpr uint32_t jwplcBackplaneSerialConfig(const char *text)
{
    return jwplcTextEquals(text, "8N1")   ? (uint32_t)SERIAL_8N1
           : jwplcTextEquals(text, "8E1") ? (uint32_t)SERIAL_8E1
           : jwplcTextEquals(text, "8O1") ? (uint32_t)SERIAL_8O1
                                          : 0UL;
}

#if defined(VPP_BACKPLANE_RTU_BAUD_RATE)
static constexpr uint32_t JWPLC_MODBUS_BAUD = jwplcBackplaneBaud(VPP_BACKPLANE_RTU_BAUD_RATE);
#else
static constexpr uint32_t JWPLC_MODBUS_BAUD = 115200UL;
#endif

#if defined(VPP_BACKPLANE_RTU_SERIAL_FORMAT)
static constexpr uint32_t JWPLC_MODBUS_CONFIG = jwplcBackplaneSerialConfig(VPP_BACKPLANE_RTU_SERIAL_FORMAT);
#else
static constexpr uint32_t JWPLC_MODBUS_CONFIG = (uint32_t)SERIAL_8N1;
#endif

static_assert(JWPLC_MODBUS_BAUD != 0UL,
              "JWPLC Backplane: baudrate RS-485 no soportado (9600/19200/38400/57600/115200)");
static_assert(JWPLC_MODBUS_CONFIG != 0UL,
              "JWPLC Backplane: formato serie RS-485 no soportado (8N1/8E1/8O1)");

// El RS-485 del Backplane (Serial2) es exclusivo del Master RTU. Un servidor
// Modbus RTU de Device > Modbus en Serial2 reabriria el mismo UART con su
// propio baudrate y consumiria las respuestas de los modulos: todos quedarian
// fuera de linea y sus salidas en fail-safe. El editor lo rechaza antes de
// compilar (exclusiveSerialPort del modulo); esto cubre editores que no lo
// validan. El debugger va por USB (Serial0).
#if defined(VPP_MODULE_CONFIG_ENTRIES_COUNT) && (VPP_MODULE_CONFIG_ENTRIES_COUNT > 0) &&     defined(VPP_MODBUS_RTU_ENABLED) && defined(VPP_MODBUS_RTU_RTU_INTERFACE)
static_assert(!(VPP_MODBUS_RTU_ENABLED) || !jwplcTextEquals(VPP_MODBUS_RTU_RTU_INTERFACE, "Serial2"),
              "JWPLC Backplane: Modbus RTU (Device > Modbus) no puede usar RS-485 (Serial2) con modulos Remote I/O; usar USB (Serial0)");
#endif

struct JWPLCIecBitAddress
{
    bool valid;
    uint16_t byteIndex;
    uint8_t bitIndex;
};

struct JWPLCVppIoEntry
{
    uint8_t slot;
    const char *channelName;
    const char *iecAddress;
};

struct JWPLCVppModuleConfigEntry
{
    uint8_t slot;
    const uint8_t *bytes;
    size_t byteCount;
};

#if defined(VPP_IO_MAPPING_ENTRIES_COUNT) && (VPP_IO_MAPPING_ENTRIES_COUNT > 0)
#define JWPLC_VPP_IO_ENTRY(i)                                                            \
    {                                                                                    \
        (uint8_t)VPP_IO_MAPPING_ENTRIES_##i##_SLOT,                                      \
        VPP_IO_MAPPING_ENTRIES_##i##_CHANNELNAME,                                        \
        VPP_IO_MAPPING_ENTRIES_##i##_IECADDRESS                                          \
    },

static const JWPLCVppIoEntry jwplcVppIoEntries[] = {
    VPP_IO_MAPPING_ENTRIES_FOREACH(JWPLC_VPP_IO_ENTRY)
};

#undef JWPLC_VPP_IO_ENTRY
#endif

#if defined(VPP_MODULE_CONFIG_ENTRIES_COUNT) && (VPP_MODULE_CONFIG_ENTRIES_COUNT > 0)
#define JWPLC_DECLARE_MODULE_CONFIG_BYTES(i)                                             \
    static const uint8_t jwplcModuleConfigBytes_##i[] =                                  \
        VPP_MODULE_CONFIG_ENTRIES_##i##_BYTES;

VPP_MODULE_CONFIG_ENTRIES_FOREACH(JWPLC_DECLARE_MODULE_CONFIG_BYTES)

#undef JWPLC_DECLARE_MODULE_CONFIG_BYTES

#define JWPLC_VPP_MODULE_CONFIG_ENTRY(i)                                                 \
    {                                                                                    \
        (uint8_t)VPP_MODULE_CONFIG_ENTRIES_##i##_SLOT,                                   \
        jwplcModuleConfigBytes_##i,                                                       \
        sizeof(jwplcModuleConfigBytes_##i)                                               \
    },

static const JWPLCVppModuleConfigEntry jwplcVppModuleConfigEntries[] = {
    VPP_MODULE_CONFIG_ENTRIES_FOREACH(JWPLC_VPP_MODULE_CONFIG_ENTRY)
};

#undef JWPLC_VPP_MODULE_CONFIG_ENTRY
#endif

// Estado por modulo Remote I/O. El Master atiende los slots en round-robin:
// un ciclo completo FC15 -> FC01 -> FC02 por slot en linea, o solo un
// sondeo FC02 por slot fuera de linea, y pasa al siguiente.
struct JWPLCRemoteSlot
{
    uint8_t slot;
    uint8_t slaveId;
    JWPLCIecBitAddress inputMap[JWPLC_REMOTE_CHANNELS];
    JWPLCIecBitAddress outputMap[JWPLC_REMOTE_CHANNELS];

    // Buffer de recepcion FC02. Solo se aplica a %IX si FC02 tuvo exito.
    uint8_t inputBits;

    // Snapshot exacto entregado a FC15.
    // Debe mantenerse separado del valor IEC actual porque el Ladder puede
    // cambiar mientras la transaccion Modbus sigue en vuelo.
    uint8_t outputBits;

    // Feedback real de las coils del Slave leido por FC01.
    // No modifica %QX ni cambia la semantica del comando IEC.
    uint8_t feedbackBits;
    uint8_t feedbackMismatchBits;
    bool feedbackValid;
    uint32_t feedbackMismatchCount;

    bool online;
    uint8_t consecutiveFailures;

    // Entradas del modulo forzadas por el debugger, para su pantalla IDLE.
    // appliedInputs: ultimo valor que el HAL escribio en los %IX del modulo.
    // inputForceOverlay: mascara << 8 | valores, calculado tras el programa.
    // sentInputForceOverlay: ultimo valor confirmado por el modulo (HR 0).
    uint8_t appliedInputs;
    uint16_t inputForceOverlay;
    uint16_t pendingInputForceOverlay;
    uint16_t sentInputForceOverlay;
    bool inputForceOverlaySent;
    bool inputForceWriteAttempted;
    uint32_t lastInputForceWriteMs;
};

static JWPLCRemoteSlot jwplcRemoteSlots[JWPLC_REMOTE_MAX_SLOTS] = {};
static uint8_t jwplcRemoteSlotCount = 0;
static uint8_t jwplcRemoteCurrent = 0;
static bool jwplcRemoteEnabled = false;

#if JWPLC_ALPHA7_RTU_TIMING_DIAGNOSTICS
struct JWPLCRtuTimingStat
{
    uint32_t lastUs = 0;
    uint32_t maxUs = 0;
    uint64_t totalUs = 0;
    uint32_t count = 0;
};

static JWPLCRtuTimingStat jwplcTimingScanPeriod;
static JWPLCRtuTimingStat jwplcTimingServiceGap;
static JWPLCRtuTimingStat jwplcTimingFc01Rtt;
static JWPLCRtuTimingStat jwplcTimingFc02Rtt;
static JWPLCRtuTimingStat jwplcTimingFc02PollPeriod;
static JWPLCRtuTimingStat jwplcTimingInputToOutputUpdate;
static JWPLCRtuTimingStat jwplcTimingInputToRemoteQ;
static JWPLCRtuTimingStat jwplcTimingRemoteQToFc15;
static JWPLCRtuTimingStat jwplcTimingFc15Rtt;
static JWPLCRtuTimingStat jwplcTimingFc15Cycle;

static uint32_t jwplcTimingLastScanUs = 0;
static uint32_t jwplcTimingLastServiceUs = 0;
static uint32_t jwplcTimingFc01StartUs = 0;
static uint32_t jwplcTimingFc02StartUs = 0;
static uint32_t jwplcTimingLastFc02StartUs = 0;
static uint32_t jwplcTimingFc15StartUs = 0;
static uint32_t jwplcTimingLastFc15StartUs = 0;
static uint32_t jwplcTimingInputChangeUs = 0;
static uint32_t jwplcTimingRemoteQChangeUs = 0;
static uint32_t jwplcTimingLastPrintMs = 0;

static bool jwplcTimingInputInitialized = false;
static bool jwplcTimingRemoteQInitialized = false;
static bool jwplcTimingPendingInputToOutputUpdate = false;
static bool jwplcTimingPendingInputToRemoteQ = false;
static bool jwplcTimingPendingRemoteQToFc15 = false;
static uint8_t jwplcTimingLastInputBits = 0;
static uint8_t jwplcTimingLastRemoteQBits = 0;

static uint32_t jwplcTimingInputChanges = 0;
static uint32_t jwplcTimingRemoteQChanges = 0;
static uint32_t jwplcTimingFc01Ok = 0;
static uint32_t jwplcTimingFc01Fail = 0;
static uint32_t jwplcTimingFc02Ok = 0;
static uint32_t jwplcTimingFc02Fail = 0;
static uint32_t jwplcTimingFc15Ok = 0;
static uint32_t jwplcTimingFc15Fail = 0;

static inline void jwplcTimingRecord(JWPLCRtuTimingStat &stat, uint32_t valueUs)
{
    stat.lastUs = valueUs;
    if (valueUs > stat.maxUs)
    {
        stat.maxUs = valueUs;
    }
    stat.totalUs += valueUs;
    ++stat.count;
}

static inline uint32_t jwplcTimingAverage(const JWPLCRtuTimingStat &stat)
{
    return stat.count == 0 ? 0U : (uint32_t)(stat.totalUs / stat.count);
}

static void jwplcTimingInit()
{
    // Durante este probe el Modbus RTU local/debugger de Serial0 debe estar OFF.
    // El Backplane sigue exclusivamente sobre Serial2.
    Serial.begin(115200);
    delay(20);
    Serial.println();
    Serial.println("[RTU-TIMING] alpha18 idle-service diagnostics enabled; keep local Modbus RTU/Serial0 OFF");
}

static void jwplcTimingOnScanStart()
{
    const uint32_t nowUs = micros();
    if (jwplcTimingLastScanUs != 0)
    {
        jwplcTimingRecord(jwplcTimingScanPeriod, nowUs - jwplcTimingLastScanUs);
    }
    jwplcTimingLastScanUs = nowUs;
}

static void jwplcTimingOnService()
{
    const uint32_t nowUs = micros();
    if (jwplcTimingLastServiceUs != 0)
    {
        jwplcTimingRecord(jwplcTimingServiceGap, nowUs - jwplcTimingLastServiceUs);
    }
    jwplcTimingLastServiceUs = nowUs;
}

static void jwplcTimingObserveRemoteInputs(uint8_t bits)
{
    if (!jwplcTimingInputInitialized)
    {
        jwplcTimingLastInputBits = bits;
        jwplcTimingInputInitialized = true;
        return;
    }

    if (bits == jwplcTimingLastInputBits)
    {
        return;
    }

    jwplcTimingLastInputBits = bits;
    jwplcTimingInputChangeUs = micros();
    jwplcTimingPendingInputToOutputUpdate = true;
    jwplcTimingPendingInputToRemoteQ = true;
    ++jwplcTimingInputChanges;
}

static void jwplcTimingOnOutputUpdate()
{
    if (!jwplcTimingPendingInputToOutputUpdate)
    {
        return;
    }

    jwplcTimingRecord(
        jwplcTimingInputToOutputUpdate,
        micros() - jwplcTimingInputChangeUs);
    jwplcTimingPendingInputToOutputUpdate = false;
}

static void jwplcTimingObserveRemoteOutputs(uint8_t bits)
{
    if (!jwplcTimingRemoteQInitialized)
    {
        jwplcTimingLastRemoteQBits = bits;
        jwplcTimingRemoteQInitialized = true;
        return;
    }

    if (bits == jwplcTimingLastRemoteQBits)
    {
        return;
    }

    const uint32_t nowUs = micros();
    jwplcTimingLastRemoteQBits = bits;
    jwplcTimingRemoteQChangeUs = nowUs;
    jwplcTimingPendingRemoteQToFc15 = true;
    ++jwplcTimingRemoteQChanges;

    if (jwplcTimingPendingInputToRemoteQ)
    {
        jwplcTimingRecord(jwplcTimingInputToRemoteQ, nowUs - jwplcTimingInputChangeUs);
        jwplcTimingPendingInputToRemoteQ = false;
    }
}

static void jwplcTimingOnFc15Accepted()
{
    const uint32_t nowUs = micros();
    if (jwplcTimingLastFc15StartUs != 0)
    {
        jwplcTimingRecord(jwplcTimingFc15Cycle, nowUs - jwplcTimingLastFc15StartUs);
    }
    jwplcTimingLastFc15StartUs = nowUs;
    jwplcTimingFc15StartUs = nowUs;

    if (jwplcTimingPendingRemoteQToFc15)
    {
        jwplcTimingRecord(jwplcTimingRemoteQToFc15, nowUs - jwplcTimingRemoteQChangeUs);
        jwplcTimingPendingRemoteQToFc15 = false;
    }
}

static void jwplcTimingOnFc15Done(bool success)
{
    if (jwplcTimingFc15StartUs != 0)
    {
        jwplcTimingRecord(jwplcTimingFc15Rtt, micros() - jwplcTimingFc15StartUs);
        jwplcTimingFc15StartUs = 0;
    }

    if (success) ++jwplcTimingFc15Ok;
    else ++jwplcTimingFc15Fail;
}

static void jwplcTimingOnFc01Accepted()
{
    jwplcTimingFc01StartUs = micros();
}

static void jwplcTimingOnFc01Done(bool success)
{
    if (jwplcTimingFc01StartUs != 0)
    {
        jwplcTimingRecord(
            jwplcTimingFc01Rtt,
            micros() - jwplcTimingFc01StartUs);

        jwplcTimingFc01StartUs = 0;
    }

    if (success) ++jwplcTimingFc01Ok;
    else ++jwplcTimingFc01Fail;
}

static void jwplcTimingOnFc02Accepted()
{
    const uint32_t nowUs = micros();
    if (jwplcTimingLastFc02StartUs != 0)
    {
        jwplcTimingRecord(jwplcTimingFc02PollPeriod, nowUs - jwplcTimingLastFc02StartUs);
    }
    jwplcTimingLastFc02StartUs = nowUs;
    jwplcTimingFc02StartUs = nowUs;
}

static void jwplcTimingOnFc02Done(bool success)
{
    if (jwplcTimingFc02StartUs != 0)
    {
        jwplcTimingRecord(jwplcTimingFc02Rtt, micros() - jwplcTimingFc02StartUs);
        jwplcTimingFc02StartUs = 0;
    }

    if (success) ++jwplcTimingFc02Ok;
    else ++jwplcTimingFc02Fail;
}

static void jwplcTimingMaybePrint()
{
    const uint32_t nowMs = millis();
    if ((uint32_t)(nowMs - jwplcTimingLastPrintMs) < 2000U)
    {
        return;
    }
    jwplcTimingLastPrintMs = nowMs;

    Serial.printf(
        "[RTU-TIMING] scan_us=%lu/%lu/%lu(n=%lu) service_gap_us=%lu/%lu/%lu(n=%lu) fc02_poll_us=%lu/%lu/%lu(n=%lu)\r\n",
        (unsigned long)jwplcTimingScanPeriod.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingScanPeriod),
        (unsigned long)jwplcTimingScanPeriod.maxUs,
        (unsigned long)jwplcTimingScanPeriod.count,
        (unsigned long)jwplcTimingServiceGap.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingServiceGap),
        (unsigned long)jwplcTimingServiceGap.maxUs,
        (unsigned long)jwplcTimingServiceGap.count,
        (unsigned long)jwplcTimingFc02PollPeriod.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingFc02PollPeriod),
        (unsigned long)jwplcTimingFc02PollPeriod.maxUs,
        (unsigned long)jwplcTimingFc02PollPeriod.count);

    Serial.printf(
        "[RTU-TIMING] fc02_rtt_us=%lu/%lu/%lu in2out_us=%lu/%lu/%lu in2q_us=%lu/%lu/%lu q2fc15_us=%lu/%lu/%lu fc15_rtt_us=%lu/%lu/%lu fc15_cycle_us=%lu/%lu/%lu changes_in=%lu changes_q=%lu ok/fail_fc02=%lu/%lu ok/fail_fc15=%lu/%lu\r\n",
        (unsigned long)jwplcTimingFc02Rtt.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingFc02Rtt),
        (unsigned long)jwplcTimingFc02Rtt.maxUs,
        (unsigned long)jwplcTimingInputToOutputUpdate.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingInputToOutputUpdate),
        (unsigned long)jwplcTimingInputToOutputUpdate.maxUs,
        (unsigned long)jwplcTimingInputToRemoteQ.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingInputToRemoteQ),
        (unsigned long)jwplcTimingInputToRemoteQ.maxUs,
        (unsigned long)jwplcTimingRemoteQToFc15.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingRemoteQToFc15),
        (unsigned long)jwplcTimingRemoteQToFc15.maxUs,
        (unsigned long)jwplcTimingFc15Rtt.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingFc15Rtt),
        (unsigned long)jwplcTimingFc15Rtt.maxUs,
        (unsigned long)jwplcTimingFc15Cycle.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingFc15Cycle),
        (unsigned long)jwplcTimingFc15Cycle.maxUs,
        (unsigned long)jwplcTimingInputChanges,
        (unsigned long)jwplcTimingRemoteQChanges,
        (unsigned long)jwplcTimingFc02Ok,
        (unsigned long)jwplcTimingFc02Fail,
        (unsigned long)jwplcTimingFc15Ok,
        (unsigned long)jwplcTimingFc15Fail);

    Serial.printf(
        "[RTU-FEEDBACK] fc01_rtt_us=%lu/%lu/%lu ok/fail=%lu/%lu slots=%u\r\n",
        (unsigned long)jwplcTimingFc01Rtt.lastUs,
        (unsigned long)jwplcTimingAverage(jwplcTimingFc01Rtt),
        (unsigned long)jwplcTimingFc01Rtt.maxUs,
        (unsigned long)jwplcTimingFc01Ok,
        (unsigned long)jwplcTimingFc01Fail,
        (unsigned int)jwplcRemoteSlotCount);

    for (uint8_t i = 0; i < jwplcRemoteSlotCount; ++i)
    {
        const JWPLCRemoteSlot &remote = jwplcRemoteSlots[i];
        Serial.printf(
            "[RTU-SLOT] slot=%u id=%u online=%u fails=%u valid=%u requested=0x%02X feedback=0x%02X mismatch=0x%02X mismatch_count=%lu\r\n",
            (unsigned int)remote.slot,
            (unsigned int)remote.slaveId,
            remote.online ? 1U : 0U,
            (unsigned int)remote.consecutiveFailures,
            remote.feedbackValid ? 1U : 0U,
            (unsigned int)remote.outputBits,
            (unsigned int)remote.feedbackBits,
            (unsigned int)remote.feedbackMismatchBits,
            (unsigned long)remote.feedbackMismatchCount);
    }
}
#else
static inline void jwplcTimingInit() {}
static inline void jwplcTimingOnScanStart() {}
static inline void jwplcTimingOnService() {}
static inline void jwplcTimingObserveRemoteInputs(uint8_t) {}
static inline void jwplcTimingOnOutputUpdate() {}
static inline void jwplcTimingObserveRemoteOutputs(uint8_t) {}
static inline void jwplcTimingOnFc15Accepted() {}
static inline void jwplcTimingOnFc15Done(bool) {}
static inline void jwplcTimingOnFc01Accepted() {}
static inline void jwplcTimingOnFc01Done(bool) {}
static inline void jwplcTimingOnFc02Accepted() {}
static inline void jwplcTimingOnFc02Done(bool) {}
static inline void jwplcTimingMaybePrint() {}
#endif

static inline bool jwplcValidPin(uint16_t pin)
{
    // OpenPLC suele usar -1 o 99 en otros targets para pines no usados.
    // En uint16_t, -1 se convierte en 0xFFFF.
    return pin != 0xFFFF && pin != 99;
}

static bool jwplcParseIecBitAddress(
    const char *address,
    char direction,
    uint16_t &byteIndex,
    uint8_t &bitIndex)
{
    if (address == NULL ||
        address[0] != '%' ||
        address[1] != direction ||
        address[2] != 'X')
    {
        return false;
    }

    char *end = NULL;
    const unsigned long parsedByte = strtoul(address + 3, &end, 10);

    if (end == address + 3 || end == NULL || *end != '.' || parsedByte > 0xFFFFUL)
    {
        return false;
    }

    char *bitEnd = NULL;
    const unsigned long parsedBit = strtoul(end + 1, &bitEnd, 10);

    if (bitEnd == end + 1 || bitEnd == NULL || *bitEnd != '\0' || parsedBit > 7UL)
    {
        return false;
    }

    byteIndex = (uint16_t)parsedByte;
    bitIndex = (uint8_t)parsedBit;
    return true;
}

static int8_t jwplcParseRemoteChannelIndex(const char *channelName, char direction)
{
    if (channelName == NULL || channelName[0] != direction)
    {
        return -1;
    }

    const char *separator = strrchr(channelName, '_');
    if (separator == NULL || separator[1] == '\0')
    {
        return -1;
    }

    char *end = NULL;
    const unsigned long index = strtoul(separator + 1, &end, 10);

    if (end == separator + 1 || end == NULL || *end != '\0' || index >= JWPLC_REMOTE_CHANNELS)
    {
        return -1;
    }

    return (int8_t)index;
}

static bool jwplcBuildRemoteMappingForSlot(JWPLCRemoteSlot &remote)
{
#if defined(VPP_IO_MAPPING_ENTRIES_COUNT) && (VPP_IO_MAPPING_ENTRIES_COUNT > 0)
    memset(remote.inputMap, 0, sizeof(remote.inputMap));
    memset(remote.outputMap, 0, sizeof(remote.outputMap));

    uint8_t inputCount = 0;
    uint8_t outputCount = 0;

    for (size_t i = 0; i < sizeof(jwplcVppIoEntries) / sizeof(jwplcVppIoEntries[0]); ++i)
    {
        const JWPLCVppIoEntry &entry = jwplcVppIoEntries[i];
        if (entry.slot != remote.slot)
        {
            continue;
        }

        const int8_t inputIndex = jwplcParseRemoteChannelIndex(entry.channelName, 'I');
        if (inputIndex >= 0)
        {
            uint16_t byteIndex = 0;
            uint8_t bitIndex = 0;
            if (jwplcParseIecBitAddress(entry.iecAddress, 'I', byteIndex, bitIndex))
            {
                JWPLCIecBitAddress &mapping = remote.inputMap[(uint8_t)inputIndex];
                if (!mapping.valid)
                {
                    ++inputCount;
                }
                mapping.valid = true;
                mapping.byteIndex = byteIndex;
                mapping.bitIndex = bitIndex;
            }
            continue;
        }

        const int8_t outputIndex = jwplcParseRemoteChannelIndex(entry.channelName, 'Q');
        if (outputIndex >= 0)
        {
            uint16_t byteIndex = 0;
            uint8_t bitIndex = 0;
            if (jwplcParseIecBitAddress(entry.iecAddress, 'Q', byteIndex, bitIndex))
            {
                JWPLCIecBitAddress &mapping = remote.outputMap[(uint8_t)outputIndex];
                if (!mapping.valid)
                {
                    ++outputCount;
                }
                mapping.valid = true;
                mapping.byteIndex = byteIndex;
                mapping.bitIndex = bitIndex;
            }
        }
    }

    return inputCount == JWPLC_REMOTE_CHANNELS &&
           outputCount == JWPLC_REMOTE_CHANNELS;
#else
    (void)remote;
    return false;
#endif
}

static bool jwplcSlaveIdInUse(uint8_t slaveId)
{
    for (uint8_t i = 0; i < jwplcRemoteSlotCount; ++i)
    {
        if (jwplcRemoteSlots[i].slaveId == slaveId)
        {
            return true;
        }
    }
    return false;
}

// Carga todos los slots Remote I/O validos del Backplane, en orden de slot.
// Un slot se descarta si:
//   - su Slave ID esta fuera de 1..247 o repite el de un slot anterior;
//   - el allocator no dejo sus 8 DI + 8 DO resolubles a %IX/%QX.
// El Editor valida ambos casos antes de compilar; esta es la defensa final.
static bool jwplcLoadRemoteSlots()
{
    jwplcRemoteSlotCount = 0;

#if defined(VPP_MODULE_CONFIG_ENTRIES_COUNT) && (VPP_MODULE_CONFIG_ENTRIES_COUNT > 0)
    for (size_t i = 0;
         i < sizeof(jwplcVppModuleConfigEntries) / sizeof(jwplcVppModuleConfigEntries[0]) &&
         jwplcRemoteSlotCount < JWPLC_REMOTE_MAX_SLOTS;
         ++i)
    {
        const JWPLCVppModuleConfigEntry &entry = jwplcVppModuleConfigEntries[i];
        if (entry.byteCount < 1)
        {
            continue;
        }

        const uint8_t slaveId = entry.bytes[0];
        if (slaveId == 0 || slaveId > 247 || jwplcSlaveIdInUse(slaveId))
        {
            continue;
        }

        JWPLCRemoteSlot &remote = jwplcRemoteSlots[jwplcRemoteSlotCount];
        memset(&remote, 0, sizeof(remote));
        remote.slot = entry.slot;
        remote.slaveId = slaveId;

        if (!jwplcBuildRemoteMappingForSlot(remote))
        {
            continue;
        }

        // Arranque optimista: el primer ciclo escribe salidas y lee entradas.
        // Si el modulo no responde, pasa a fuera de linea tras
        // JWPLC_REMOTE_OFFLINE_FAILURES intentos.
        remote.online = true;
        ++jwplcRemoteSlotCount;
    }
#endif

    return jwplcRemoteSlotCount > 0;
}

static void jwplcApplyRemoteInputs(JWPLCRemoteSlot &remote, uint8_t bits)
{
    remote.appliedInputs = bits;

    for (uint8_t i = 0; i < JWPLC_REMOTE_CHANNELS; ++i)
    {
        const JWPLCIecBitAddress &mapping = remote.inputMap[i];
        if (!mapping.valid || bool_input[mapping.byteIndex][mapping.bitIndex] == NULL)
        {
            continue;
        }

        *bool_input[mapping.byteIndex][mapping.bitIndex] =
            (bits & (uint8_t)(1U << i)) != 0;
    }
}

static uint8_t jwplcPackRemoteOutputs(const JWPLCRemoteSlot &remote)
{
    uint8_t packed = 0;

    for (uint8_t i = 0; i < JWPLC_REMOTE_CHANNELS; ++i)
    {
        const JWPLCIecBitAddress &mapping = remote.outputMap[i];
        if (!mapping.valid || bool_output[mapping.byteIndex][mapping.bitIndex] == NULL)
        {
            continue;
        }

        if (*bool_output[mapping.byteIndex][mapping.bitIndex])
        {
            packed |= (uint8_t)(1U << i);
        }
    }

    return packed;
}

static void jwplcUpdateRemoteFeedback(JWPLCRemoteSlot &remote, bool valid)
{
    remote.feedbackValid = valid;

    if (!valid)
    {
        // Si FC01 fallo, el ultimo bitmap no debe presentarse como una
        // comparacion nueva y valida.
        remote.feedbackMismatchBits = 0;
        return;
    }

    // Comparar contra el snapshot que realmente fue enviado mediante FC15.
    // No leer bool_output nuevamente: puede corresponder ya al siguiente scan.
    remote.feedbackMismatchBits =
        (uint8_t)(remote.feedbackBits ^ remote.outputBits);

    if (remote.feedbackMismatchBits != 0)
    {
        ++remote.feedbackMismatchCount;
    }
}

// Registra el resultado de una transaccion del slot actual.
static void jwplcRecordRemoteResult(JWPLCRemoteSlot &remote, bool success)
{
    if (success)
    {
        remote.consecutiveFailures = 0;
        remote.online = true;
        return;
    }

    if (remote.consecutiveFailures < 0xFF)
    {
        ++remote.consecutiveFailures;
    }

    if (remote.online && remote.consecutiveFailures >= JWPLC_REMOTE_OFFLINE_FAILURES)
    {
        // Perdida de comunicacion: las entradas remotas pasan a estado
        // seguro (0) para que el programa no actue sobre valores congelados.
        remote.online = false;
        remote.feedbackValid = false;
        remote.feedbackMismatchBits = 0;
        jwplcApplyRemoteInputs(remote, 0);
        // Al volver, el modulo arranca sin marca: reenviar el estado actual.
        remote.inputForceOverlaySent = false;
    }
}

// Escritura FC06 de las entradas forzadas pendiente para este slot.
static bool jwplcRemoteInputForceWriteDue(const JWPLCRemoteSlot &remote)
{
    if (!remote.online)
        return false;

    const uint32_t elapsedMs = (uint32_t)(millis() - remote.lastInputForceWriteMs);

    if (!remote.inputForceOverlaySent)
    {
        // Primer envio, o el modulo no lo confirmo (p. ej. firmware anterior).
        return !remote.inputForceWriteAttempted ||
               elapsedMs >= JWPLC_REMOTE_INPUT_FORCE_REFRESH_MS;
    }

    if (remote.sentInputForceOverlay != remote.inputForceOverlay)
        return true;

    return remote.inputForceOverlay != 0 &&
           elapsedMs >= JWPLC_REMOTE_INPUT_FORCE_REFRESH_MS;
}

enum JWPLCRemoteRtuPhase : uint8_t
{
    JWPLC_REMOTE_WRITE_START = 0,
    JWPLC_REMOTE_WRITE_WAIT,
    JWPLC_REMOTE_FEEDBACK_START,
    JWPLC_REMOTE_FEEDBACK_WAIT,
    JWPLC_REMOTE_READ_START,
    JWPLC_REMOTE_READ_WAIT,
    // FC06 de entradas forzadas, solo cuando hace falta (ver HR 0).
    JWPLC_REMOTE_FORCE_START,
    JWPLC_REMOTE_FORCE_WAIT,
    // Todos los slots fuera de linea y aun no toca sondear: bus en reposo.
    JWPLC_REMOTE_IDLE
};

static JWPLCRemoteRtuPhase jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;

static uint32_t jwplcLastOfflineProbeMs = 0;
static bool jwplcOfflineProbeStarted = false;

// Pasa al siguiente slot en round-robin. Los slots en linea reciben su ciclo
// completo; un slot fuera de linea solo recibe un sondeo FC02 y como maximo
// uno por JWPLC_REMOTE_OFFLINE_PROBE_INTERVAL_MS en todo el bus, para que los
// modulos desconectados no retrasen las escrituras a los modulos sanos.
static void jwplcAdvanceRemoteSlot()
{
    const uint32_t nowMs = millis();
    const bool probeDue =
        !jwplcOfflineProbeStarted ||
        (uint32_t)(nowMs - jwplcLastOfflineProbeMs) >= JWPLC_REMOTE_OFFLINE_PROBE_INTERVAL_MS;

    for (uint8_t step = 1; step <= jwplcRemoteSlotCount; ++step)
    {
        const uint8_t index = (uint8_t)((jwplcRemoteCurrent + step) % jwplcRemoteSlotCount);
        if (jwplcRemoteSlots[index].online)
        {
            jwplcRemoteCurrent = index;
            jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;
            return;
        }

        if (probeDue)
        {
            jwplcRemoteCurrent = index;
            jwplcRemotePhase = JWPLC_REMOTE_READ_START;
            jwplcLastOfflineProbeMs = nowMs;
            jwplcOfflineProbeStarted = true;
            return;
        }
    }

    jwplcRemotePhase = JWPLC_REMOTE_IDLE;
}

static void jwplcServiceRemoteRtu()
{
    if (!jwplcRemoteEnabled)
    {
        return;
    }

    JWPLC_ModbusRTU.task();
    jwplcTimingOnService();

    JWPLCRemoteSlot &remote = jwplcRemoteSlots[jwplcRemoteCurrent];
    // El probe de timing solo sigue al primer slot para conservar sus metricas.
    const bool timingSlot = (jwplcRemoteCurrent == 0);

    switch (jwplcRemotePhase)
    {
    case JWPLC_REMOTE_WRITE_START:
        remote.outputBits = jwplcPackRemoteOutputs(remote);
        if (JWPLC_ModbusRTU.writeMultipleCoils(
                remote.slaveId,
                0,
                JWPLC_REMOTE_CHANNELS,
                &remote.outputBits,
                JWPLC_MODBUS_TIMEOUT_MS))
        {
            if (timingSlot) jwplcTimingOnFc15Accepted();
            jwplcRemotePhase = JWPLC_REMOTE_WRITE_WAIT;
        }
        break;

    case JWPLC_REMOTE_WRITE_WAIT:
        if (JWPLC_ModbusRTU.masterDone())
        {
            const bool fc15Succeeded = JWPLC_ModbusRTU.masterSucceeded();
            if (timingSlot) jwplcTimingOnFc15Done(fc15Succeeded);
            JWPLC_ModbusRTU.clearMasterResult();
            jwplcRecordRemoteResult(remote, fc15Succeeded);

            // Un fallo corta el ciclo de este slot: no encadenar timeouts
            // FC01/FC02 contra un modulo que no responde.
            if (fc15Succeeded)
            {
                jwplcRemotePhase = JWPLC_REMOTE_FEEDBACK_START;
            }
            else
            {
                jwplcAdvanceRemoteSlot();
            }
        }
        break;

    case JWPLC_REMOTE_FEEDBACK_START:
        // Cada FC01 debe volver a validar su propia muestra.
        // No conservar como valido un feedback de un ciclo anterior mientras
        // la nueva lectura esta pendiente o no pudo iniciarse.
        remote.feedbackValid = false;
        remote.feedbackMismatchBits = 0;
        remote.feedbackBits = 0;

        if (JWPLC_ModbusRTU.readCoils(
                remote.slaveId,
                0,
                JWPLC_REMOTE_CHANNELS,
                &remote.feedbackBits,
                JWPLC_MODBUS_TIMEOUT_MS))
        {
            if (timingSlot) jwplcTimingOnFc01Accepted();
            jwplcRemotePhase = JWPLC_REMOTE_FEEDBACK_WAIT;
        }
        break;

    case JWPLC_REMOTE_FEEDBACK_WAIT:
        if (JWPLC_ModbusRTU.masterDone())
        {
            const bool fc01Succeeded = JWPLC_ModbusRTU.masterSucceeded();
            if (timingSlot) jwplcTimingOnFc01Done(fc01Succeeded);
            JWPLC_ModbusRTU.clearMasterResult();
            jwplcUpdateRemoteFeedback(remote, fc01Succeeded);
            jwplcRecordRemoteResult(remote, fc01Succeeded);

            if (fc01Succeeded)
            {
                jwplcRemotePhase = JWPLC_REMOTE_READ_START;
            }
            else
            {
                jwplcAdvanceRemoteSlot();
            }
        }
        break;

    case JWPLC_REMOTE_READ_START:
        remote.inputBits = 0;
        if (JWPLC_ModbusRTU.readDiscreteInputs(
                remote.slaveId,
                0,
                JWPLC_REMOTE_CHANNELS,
                &remote.inputBits,
                JWPLC_MODBUS_TIMEOUT_MS))
        {
            if (timingSlot) jwplcTimingOnFc02Accepted();
            jwplcRemotePhase = JWPLC_REMOTE_READ_WAIT;
        }
        break;

    case JWPLC_REMOTE_READ_WAIT:
        if (JWPLC_ModbusRTU.masterDone())
        {
            const bool fc02Succeeded = JWPLC_ModbusRTU.masterSucceeded();
            if (timingSlot) jwplcTimingOnFc02Done(fc02Succeeded);
            JWPLC_ModbusRTU.clearMasterResult();
            jwplcRecordRemoteResult(remote, fc02Succeeded);

            if (fc02Succeeded)
            {
                if (timingSlot) jwplcTimingObserveRemoteInputs(remote.inputBits);
                jwplcApplyRemoteInputs(remote, remote.inputBits);
            }

            if (fc02Succeeded && jwplcRemoteInputForceWriteDue(remote))
            {
                jwplcRemotePhase = JWPLC_REMOTE_FORCE_START;
            }
            else
            {
                jwplcAdvanceRemoteSlot();
            }
        }
        break;

    case JWPLC_REMOTE_FORCE_START:
        remote.pendingInputForceOverlay = remote.inputForceOverlay;
        if (JWPLC_ModbusRTU.writeSingleRegister(
                remote.slaveId,
                JWPLC_REMOTE_HR_INPUT_FORCE,
                remote.pendingInputForceOverlay,
                JWPLC_MODBUS_TIMEOUT_MS))
        {
            remote.inputForceWriteAttempted = true;
            remote.lastInputForceWriteMs = millis();
            jwplcRemotePhase = JWPLC_REMOTE_FORCE_WAIT;
        }
        break;

    case JWPLC_REMOTE_FORCE_WAIT:
        if (JWPLC_ModbusRTU.masterDone())
        {
            // Solo afecta a la pantalla del modulo: un fallo no cuenta para
            // fuera de linea (un firmware anterior responde excepcion).
            if (JWPLC_ModbusRTU.masterSucceeded())
            {
                remote.sentInputForceOverlay = remote.pendingInputForceOverlay;
                remote.inputForceOverlaySent = true;
            }
            JWPLC_ModbusRTU.clearMasterResult();
            jwplcAdvanceRemoteSlot();
        }
        break;

    case JWPLC_REMOTE_IDLE:
        jwplcAdvanceRemoteSlot();
        break;

    default:
        jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;
        break;
    }
}

// Alpha7.18: atiende el Backplane durante el tiempo ocioso del scan.
// Es cooperativo, single-thread y no bloqueante. Los hooks existentes en
// updateInputBuffers/updateOutputBuffers se conservan como puntos de respaldo.
static constexpr uint32_t JWPLC_REMOTE_IDLE_SERVICE_PERIOD_US = 1000UL;

void hardwareService()
{
    if (!jwplcRemoteEnabled)
    {
        return;
    }

    static uint32_t lastIdleServiceUs = 0;
    const uint32_t nowUs = micros();
    if (lastIdleServiceUs != 0 &&
        (uint32_t)(nowUs - lastIdleServiceUs) < JWPLC_REMOTE_IDLE_SERVICE_PERIOD_US)
    {
        return;
    }

    lastIdleServiceUs = nowUs;
    jwplcServiceRemoteRtu();
}

void hardwareInit()
{
    jwplcTimingInit();
    // JWPLC Basic v2.x:
    // La inicializacion de E/S industriales ya la realiza el core jwcontrol
    // mediante initPeripherals(), antes de que se ejecute setup().
    //
    // No tocar EN_IO aqui.
    // No repetir pinMode() aqui.
    // No reinicializar TCA6424A aqui.

    // Alpha12: solo se habilita el Master RTU si el VPP genero al menos un
    // slot Remote I/O valido (Slave ID unico y 8 DI + 8 DO resolubles).
    // El Backplane sigue siendo la fuente de verdad; sin slots remotos
    // Serial2 queda libre y el JWPLC se comporta como un controlador local.
    if (jwplcLoadRemoteSlots() &&
        JWPLC_ModbusRTU.begin(
            JWPLC_MODBUS_MASTER_LOCAL_ID,
            JWPLC_MODBUS_BAUD,
            JWPLC_MODBUS_CONFIG))
    {
        // Alpha12: read...()/write...() bloquean o no segun el motor. El scan
        // PLC nunca debe esperar al bus, asi que ASYNC se fija explicitamente
        // aunque ya sea el default del package.
        if (!JWPLC_ModbusRTU.motor(ASYNC))
        {
            JWPLC_ModbusRTU.end();
            return;
        }

        JWPLC_ModbusRTU.setFrameGapMs(JWPLC_MODBUS_FRAME_GAP_MS);

        jwplcRemoteCurrent = 0;
        jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;
        jwplcRemoteEnabled = true;

#if JWPLC_ALPHA7_RTU_TIMING_DIAGNOSTICS
        Serial.printf(
            "[RTU-FEEDBACK] Backplane master enabled: slots=%u baud=%lu; %%QX remains command state\r\n",
            (unsigned int)jwplcRemoteSlotCount,
            (unsigned long)JWPLC_MODBUS_BAUD);
#endif
    }
}

// =====================================================
// Entradas locales forzadas -> pantalla IDLE
// =====================================================
// Un forzado del debugger vive en la variable IEC: la lectura física no
// cambia y la IDLE no lo mostraría. El runtime re-impone el valor forzado
// sobre el valor crudo justo después de updateInputBuffers()
// (runtime_apply_located_forces), así que en updateOutputBuffers() el valor
// crudo de cada entrada ya es el que ve el programa.
//
// Detección exacta si el runtime del editor exporta
// runtime_located_bool_is_forced(). Si no, se marca forzada la entrada cuyo
// valor crudo difiere de la lectura física; un forzado al mismo valor que la
// entrada física no se distingue (la IDLE muestra igual el valor correcto).
// Ambos símbolos son weak: con un package o un editor anteriores, el HAL
// compila igual y la IDLE queda como antes.
extern "C" uint8_t runtime_located_bool_is_forced(const void *raw) __attribute__((weak));
extern "C" void jwplcDisplaySetInputForceOverlay(uint8_t forcedMask, uint8_t forcedValues) __attribute__((weak));

// Lectura física de I0_0..I0_7 del último updateInputBuffers() (bit i = I0_i).
static uint8_t jwplcLocalInputsPhysical = 0;

// raw: almacenamiento de la entrada (ya con el forzado re-impuesto).
// written: último valor que el HAL escribió en ella.
static bool jwplcInputIsForced(const IEC_BOOL *raw, bool written)
{
    return runtime_located_bool_is_forced
               ? runtime_located_bool_is_forced(raw) != 0
               : (*raw != 0) != written;
}

static void jwplcPublishForcedInputs()
{
    // Entradas locales -> IDLE de este equipo.
    if (jwplcDisplaySetInputForceOverlay)
    {
        uint8_t forcedMask = 0;
        uint8_t forcedValues = 0;

        // pinMask_DIN es fijo (I0_0..I0_7), así que %IX0.i es el bit i de la IDLE.
        for (int i = 0; i < NUM_DISCRETE_INPUT && i < 8; i++)
        {
            IEC_BOOL *raw = bool_input[0][i];
            if (raw == NULL || !jwplcValidPin(pinMask_DIN[i]))
                continue;

            const uint8_t bit = (uint8_t)(1U << i);
            if (jwplcInputIsForced(raw, (jwplcLocalInputsPhysical & bit) != 0))
            {
                forcedMask |= bit;
                if (*raw)
                    forcedValues |= bit;
            }
        }

        // Barata si no cambió: solo marca el display cuando el overlay varía.
        jwplcDisplaySetInputForceOverlay(forcedMask, forcedValues);
    }

    // Entradas de cada módulo Remote I/O -> IDLE del módulo, vía FC06 HR 0
    // (jwplcServiceRemoteRtu lo envía solo cuando cambia).
    for (uint8_t s = 0; s < jwplcRemoteSlotCount; ++s)
    {
        JWPLCRemoteSlot &remote = jwplcRemoteSlots[s];
        uint8_t forcedMask = 0;
        uint8_t forcedValues = 0;

        for (uint8_t i = 0; i < JWPLC_REMOTE_CHANNELS; ++i)
        {
            const JWPLCIecBitAddress &mapping = remote.inputMap[i];
            if (!mapping.valid)
                continue;

            IEC_BOOL *raw = bool_input[mapping.byteIndex][mapping.bitIndex];
            if (raw == NULL)
                continue;

            const uint8_t bit = (uint8_t)(1U << i);
            if (jwplcInputIsForced(raw, (remote.appliedInputs & bit) != 0))
            {
                forcedMask |= bit;
                if (*raw)
                    forcedValues |= bit;
            }
        }

        remote.inputForceOverlay = (uint16_t)(((uint16_t)forcedMask << 8) | forcedValues);
    }
}

void updateInputBuffers()
{
    jwplcTimingOnScanStart();
    uint8_t physical = 0;
    for (int i = 0; i < NUM_DISCRETE_INPUT; i++)
    {
        uint16_t pin = pinMask_DIN[i];

        if (bool_input[i / 8][i % 8] != NULL && jwplcValidPin(pin))
        {
            const int value = digitalRead(pin);
            *bool_input[i / 8][i % 8] = value;
            if (value && i < 8)
                physical |= (uint8_t)(1U << i);
        }
    }
    jwplcLocalInputsPhysical = physical;

    jwplcServiceRemoteRtu();
}

void updateOutputBuffers()
{
    jwplcTimingOnOutputUpdate();
    if (jwplcRemoteEnabled)
    {
        jwplcTimingObserveRemoteOutputs(jwplcPackRemoteOutputs(jwplcRemoteSlots[0]));
    }
    for (int i = 0; i < NUM_DISCRETE_OUTPUT; i++)
    {
        uint16_t pin = pinMask_DOUT[i];

        if (bool_output[i / 8][i % 8] != NULL && jwplcValidPin(pin))
        {
            digitalWrite(pin, *bool_output[i / 8][i % 8]);
        }
    }

    jwplcPublishForcedInputs();

    jwplcServiceRemoteRtu();
    jwplcTimingMaybePrint();
}
