#include <stdlib.h>
#include <string.h>

extern "C"
{
#include "openplc.h"
}

#include "Arduino.h"
#include "vpp_config.h"
#include <JWPLC_ModbusRTU.h>

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
static constexpr uint32_t JWPLC_MODBUS_BAUD = 115200UL;
static constexpr uint32_t JWPLC_MODBUS_CONFIG = SERIAL_8N1;
static constexpr uint32_t JWPLC_MODBUS_TIMEOUT_MS = 250UL;
static constexpr uint16_t JWPLC_MODBUS_FRAME_GAP_MS = 2;

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

static JWPLCIecBitAddress jwplcRemoteInputMap[JWPLC_REMOTE_CHANNELS] = {};
static JWPLCIecBitAddress jwplcRemoteOutputMap[JWPLC_REMOTE_CHANNELS] = {};
static bool jwplcRemoteEnabled = false;
static uint8_t jwplcRemoteSlaveId = 0;
static uint8_t jwplcRemoteInputBits = 0;
static uint8_t jwplcRemoteOutputBits = 0;

enum JWPLCRemoteRtuPhase : uint8_t
{
    JWPLC_REMOTE_WRITE_START = 0,
    JWPLC_REMOTE_WRITE_WAIT,
    JWPLC_REMOTE_READ_START,
    JWPLC_REMOTE_READ_WAIT
};

static JWPLCRemoteRtuPhase jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;

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

static bool jwplcBuildRemoteMappingForSlot(uint8_t slot)
{
#if defined(VPP_IO_MAPPING_ENTRIES_COUNT) && (VPP_IO_MAPPING_ENTRIES_COUNT > 0)
    memset(jwplcRemoteInputMap, 0, sizeof(jwplcRemoteInputMap));
    memset(jwplcRemoteOutputMap, 0, sizeof(jwplcRemoteOutputMap));

    uint8_t inputCount = 0;
    uint8_t outputCount = 0;

    for (size_t i = 0; i < sizeof(jwplcVppIoEntries) / sizeof(jwplcVppIoEntries[0]); ++i)
    {
        const JWPLCVppIoEntry &entry = jwplcVppIoEntries[i];
        if (entry.slot != slot)
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
                JWPLCIecBitAddress &mapping = jwplcRemoteInputMap[(uint8_t)inputIndex];
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
                JWPLCIecBitAddress &mapping = jwplcRemoteOutputMap[(uint8_t)outputIndex];
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
    (void)slot;
    return false;
#endif
}

static bool jwplcLoadFirstRemoteSlot()
{
#if defined(VPP_MODULE_CONFIG_ENTRIES_COUNT) && (VPP_MODULE_CONFIG_ENTRIES_COUNT > 0)
    for (size_t i = 0;
         i < sizeof(jwplcVppModuleConfigEntries) / sizeof(jwplcVppModuleConfigEntries[0]);
         ++i)
    {
        const JWPLCVppModuleConfigEntry &entry = jwplcVppModuleConfigEntries[i];
        if (entry.byteCount < 1)
        {
            continue;
        }

        const uint8_t slaveId = entry.bytes[0];
        if (slaveId == 0 || slaveId > 247)
        {
            continue;
        }

        if (!jwplcBuildRemoteMappingForSlot(entry.slot))
        {
            continue;
        }

        jwplcRemoteSlaveId = slaveId;
        return true;
    }
#endif

    return false;
}

static void jwplcApplyRemoteInputs()
{
    for (uint8_t i = 0; i < JWPLC_REMOTE_CHANNELS; ++i)
    {
        const JWPLCIecBitAddress &mapping = jwplcRemoteInputMap[i];
        if (!mapping.valid || bool_input[mapping.byteIndex][mapping.bitIndex] == NULL)
        {
            continue;
        }

        *bool_input[mapping.byteIndex][mapping.bitIndex] =
            (jwplcRemoteInputBits & (uint8_t)(1U << i)) != 0;
    }
}

static uint8_t jwplcPackRemoteOutputs()
{
    uint8_t packed = 0;

    for (uint8_t i = 0; i < JWPLC_REMOTE_CHANNELS; ++i)
    {
        const JWPLCIecBitAddress &mapping = jwplcRemoteOutputMap[i];
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

static void jwplcServiceRemoteRtu()
{
    if (!jwplcRemoteEnabled)
    {
        return;
    }

    JWPLC_ModbusRTU.task();

    switch (jwplcRemotePhase)
    {
    case JWPLC_REMOTE_WRITE_START:
        jwplcRemoteOutputBits = jwplcPackRemoteOutputs();
        if (JWPLC_ModbusRTU.requestWriteMultipleCoils(
                jwplcRemoteSlaveId,
                0,
                JWPLC_REMOTE_CHANNELS,
                &jwplcRemoteOutputBits,
                JWPLC_MODBUS_TIMEOUT_MS))
        {
            jwplcRemotePhase = JWPLC_REMOTE_WRITE_WAIT;
        }
        break;

    case JWPLC_REMOTE_WRITE_WAIT:
        if (JWPLC_ModbusRTU.masterDone())
        {
            JWPLC_ModbusRTU.clearMasterResult();
            jwplcRemotePhase = JWPLC_REMOTE_READ_START;
        }
        break;

    case JWPLC_REMOTE_READ_START:
        jwplcRemoteInputBits = 0;
        if (JWPLC_ModbusRTU.requestReadDiscreteInputs(
                jwplcRemoteSlaveId,
                0,
                JWPLC_REMOTE_CHANNELS,
                &jwplcRemoteInputBits,
                JWPLC_MODBUS_TIMEOUT_MS))
        {
            jwplcRemotePhase = JWPLC_REMOTE_READ_WAIT;
        }
        break;

    case JWPLC_REMOTE_READ_WAIT:
        if (JWPLC_ModbusRTU.masterDone())
        {
            if (JWPLC_ModbusRTU.masterSucceeded())
            {
                jwplcApplyRemoteInputs();
            }

            JWPLC_ModbusRTU.clearMasterResult();
            jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;
        }
        break;

    default:
        jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;
        break;
    }
}

void hardwareInit()
{
    // JWPLC Basic v2.x:
    // La inicializacion de E/S industriales ya la realiza el core jwcontrol
    // mediante initPeripherals(), antes de que se ejecute setup().
    //
    // No tocar EN_IO aqui.
    // No repetir pinMode() aqui.
    // No reinicializar TCA6424A aqui.

    // A7.3.1: solo se habilita el Master RTU si el VPP genero una
    // configuracion de Remote I/O valida y el allocator dejo 8 DI + 8 DO
    // resolubles para ese slot. El Backplane sigue siendo la fuente de verdad.
    if (jwplcLoadFirstRemoteSlot() &&
        JWPLC_ModbusRTU.begin(
            JWPLC_MODBUS_MASTER_LOCAL_ID,
            JWPLC_MODBUS_BAUD,
            JWPLC_MODBUS_CONFIG))
    {
        JWPLC_ModbusRTU.setFrameGapMs(JWPLC_MODBUS_FRAME_GAP_MS);
        jwplcRemotePhase = JWPLC_REMOTE_WRITE_START;
        jwplcRemoteEnabled = true;
    }
}

void updateInputBuffers()
{
    for (int i = 0; i < NUM_DISCRETE_INPUT; i++)
    {
        uint16_t pin = pinMask_DIN[i];

        if (bool_input[i / 8][i % 8] != NULL && jwplcValidPin(pin))
        {
            *bool_input[i / 8][i % 8] = digitalRead(pin);
        }
    }

    jwplcServiceRemoteRtu();
}

void updateOutputBuffers()
{
    for (int i = 0; i < NUM_DISCRETE_OUTPUT; i++)
    {
        uint16_t pin = pinMask_DOUT[i];

        if (bool_output[i / 8][i % 8] != NULL && jwplcValidPin(pin))
        {
            digitalWrite(pin, *bool_output[i / 8][i % 8]);
        }
    }

    jwplcServiceRemoteRtu();
}
