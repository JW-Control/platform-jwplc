from __future__ import annotations

import argparse
from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}_MATCH_COUNT={count}")
    return text.replace(old, new, 1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sketch", required=True)
    args = parser.parse_args()

    path = Path(args.sketch).resolve()
    if not path.is_file():
        raise RuntimeError(f"P5_SKETCH_NOT_FOUND={path}")

    text = path.read_text(encoding="utf-8")

    text = replace_once(
        text,
        "#include <JWPLC_ModbusTCP.h>\n",
        "#include <JWPLC_ModbusTCP.h>\n"
        "#include <JWPLC_ModbusRTU.h>\n",
        "P5_INCLUDE_RTU",
    )

    map_anchor = """static uint8_t coils[(COIL_COUNT + 7U) / 8U];
static uint16_t holdingRegisters[HOLDING_COUNT];

// ============================================================================
// Perfil FULL_RUNTIME_REALISTIC
"""

    map_replacement = """static uint8_t coils[(COIL_COUNT + 7U) / 8U];
static uint16_t holdingRegisters[HOLDING_COUNT];

// ============================================================================
// Modbus RTU simultaneo
// ============================================================================

static constexpr uint8_t RTU_SLAVE_ID = 2;
static constexpr uint32_t RTU_BAUD = 115200UL;
static constexpr uint32_t RTU_CONFIG = SERIAL_8N1;
static constexpr uint16_t RTU_BIT_COUNT = 256;
static constexpr uint16_t RTU_REGISTER_COUNT = 256;

static uint8_t rtuCoils[(RTU_BIT_COUNT + 7U) / 8U];
static uint8_t rtuDiscreteInputs[(RTU_BIT_COUNT + 7U) / 8U];
static uint16_t rtuHolding[RTU_REGISTER_COUNT];
static uint16_t rtuInput[RTU_REGISTER_COUNT];

static bool rtuReady = false;
static uint32_t rtuLastServiceUs = 0;
static uint32_t rtuServiceGapMaxUs = 0;

// ============================================================================
// Perfil FULL_RUNTIME_REALISTIC
"""

    text = replace_once(
        text,
        map_anchor,
        map_replacement,
        "P5_RTU_MAPS",
    )

    helper_anchor = """static const char *yesNo(bool value)
{
    return value ? "YES" : "NO";
}

// ============================================================================
// Display USER
"""

    helper_replacement = """static const char *yesNo(bool value)
{
    return value ? "YES" : "NO";
}

static void serviceRtu()
{
    const uint32_t nowUs = micros();

    if (rtuLastServiceUs != 0)
    {
        const uint32_t gapUs =
            (uint32_t)(nowUs - rtuLastServiceUs);

        updateMaxU32(
            gapUs,
            rtuServiceGapMaxUs);
    }

    rtuLastServiceUs = nowUs;

    JWPLC_ModbusRTU.task();
}

// ============================================================================
// Display USER
"""

    text = replace_once(
        text,
        helper_anchor,
        helper_replacement,
        "P5_RTU_SERVICE_HELPER",
    )

    ready_anchor = """static bool fullRuntimeReady()
{
    return
        JWPLC_Display.isReady() &&
        framReady &&
        sdReady &&
        (bool)sdAppendFile &&
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady() &&
        JWPLCButtons::isReady() &&
        ioReady() &&
        rtcReady();
}

static uint32_t peripheralFailureCount()
"""

    ready_replacement = """static bool fullRuntimeReady()
{
    return
        JWPLC_Display.isReady() &&
        framReady &&
        sdReady &&
        (bool)sdAppendFile &&
        JWPLCSD::isEnabled() &&
        JWPLCSD::isCardPresent() &&
        JWPLCSD::isReady() &&
        JWPLCButtons::isReady() &&
        ioReady() &&
        rtcReady();
}

static bool combinedRuntimeReady()
{
    return
        fullRuntimeReady() &&
        rtuReady &&
        JWPLC_ModbusTCP.serverReady();
}

static uint32_t peripheralFailureCount()
"""

    text = replace_once(
        text,
        ready_anchor,
        ready_replacement,
        "P5_COMBINED_READY",
    )

    reset_anchor = """static void resetPerfCounters()
{
    JWPLC_ModbusTCP.resetStats();

    loopGapSumUs = 0;
"""

    reset_replacement = """static void resetPerfCounters()
{
    JWPLC_ModbusTCP.resetStats();
    JWPLC_ModbusRTU.resetStats();

    rtuLastServiceUs = micros();
    rtuServiceGapMaxUs = 0;

    loopGapSumUs = 0;
"""

    text = replace_once(
        text,
        reset_anchor,
        reset_replacement,
        "P5_RTU_RESET_STATS",
    )

    snapshot_anchor = """    Serial.print("CLIENT_CONNECTED=");
    Serial.println(
        yesNo(
            JWPLC_ModbusTCP.clientConnected()));

    // --------------------------------------------------------
    // TFT
"""

    snapshot_replacement = """    Serial.print("CLIENT_CONNECTED=");
    Serial.println(
        yesNo(
            JWPLC_ModbusTCP.clientConnected()));

    // --------------------------------------------------------
    // Ethernet
    // --------------------------------------------------------

    Serial.print("ETH_READY=");
    Serial.println(
        yesNo(
            JWPLC_Ethernet.isReady()));

    Serial.print("ETH_LINK=");
    Serial.println(
        JWPLC_Ethernet.linkUp()
            ? "UP"
            : "DOWN");

    Serial.print("ETH_IP=");
    Serial.println(
        JWPLC_Ethernet.localIP());

    // --------------------------------------------------------
    // Modbus RTU
    // --------------------------------------------------------

    const JWPLCModbusRTUStats &rtuStats =
        JWPLC_ModbusRTU.stats();

    Serial.print("RTU_READY=");
    Serial.println(yesNo(rtuReady));

    Serial.print("RTU_SLAVE_ID=");
    Serial.println(RTU_SLAVE_ID);

    Serial.print("RTU_BAUD=");
    Serial.println(RTU_BAUD);

    Serial.print("RTU_RX_FRAMES=");
    Serial.println(rtuStats.rxFrames);

    Serial.print("RTU_TX_FRAMES=");
    Serial.println(rtuStats.txFrames);

    Serial.print("RTU_REQUESTS_OK=");
    Serial.println(rtuStats.requestsOk);

    Serial.print("RTU_CRC_ERRORS=");
    Serial.println(rtuStats.crcErrors);

    Serial.print("RTU_EXCEPTIONS_SENT=");
    Serial.println(rtuStats.exceptionsSent);

    Serial.print("RTU_MASTER_TIMEOUTS=");
    Serial.println(rtuStats.masterTimeouts);

    Serial.print("RTU_LAST_ERROR=");
    Serial.println(
        JWPLC_ModbusRTU.lastErrorString());

    Serial.print("RTU_SERVICE_GAP_MAX_US=");
    Serial.println(rtuServiceGapMaxUs);

    Serial.print("COMBINED_RUNTIME_READY=");
    Serial.println(
        yesNo(
            combinedRuntimeReady()));

    // --------------------------------------------------------
    // TFT
"""

    text = replace_once(
        text,
        snapshot_anchor,
        snapshot_replacement,
        "P5_RTU_SNAPSHOT",
    )

    setup_anchor = """    for (
        uint16_t i = 0;
        i < HOLDING_COUNT;
        ++i)
    {
        holdingRegisters[i] =
            (uint16_t)(
                0x1000U +
                i);
    }

    JWPLC_ModbusTCP.setCoils(
"""

    setup_replacement = """    for (
        uint16_t i = 0;
        i < HOLDING_COUNT;
        ++i)
    {
        holdingRegisters[i] =
            (uint16_t)(
                0x1000U +
                i);
    }

    memset(
        rtuCoils,
        0,
        sizeof(rtuCoils));

    memset(
        rtuDiscreteInputs,
        0,
        sizeof(rtuDiscreteInputs));

    for (
        uint16_t i = 0;
        i < RTU_REGISTER_COUNT;
        ++i)
    {
        rtuHolding[i] =
            (uint16_t)(
                0x2000U +
                i);

        rtuInput[i] =
            (uint16_t)(
                0x3000U +
                i);
    }

    JWPLC_ModbusRTU.setCoils(
        rtuCoils,
        RTU_BIT_COUNT);

    JWPLC_ModbusRTU.setDiscreteInputs(
        rtuDiscreteInputs,
        RTU_BIT_COUNT);

    JWPLC_ModbusRTU.setHoldingRegisters(
        rtuHolding,
        RTU_REGISTER_COUNT);

    JWPLC_ModbusRTU.setInputRegisters(
        rtuInput,
        RTU_REGISTER_COUNT);

    rtuReady =
        JWPLC_ModbusRTU.begin(
            RTU_SLAVE_ID,
            RTU_BAUD,
            RTU_CONFIG);

    JWPLC_ModbusTCP.setCoils(
"""

    text = replace_once(
        text,
        setup_anchor,
        setup_replacement,
        "P5_RTU_SETUP",
    )

    boot_anchor = """    Serial.print(
        "IO_READY_BOOT=");
    Serial.println(
        yesNo(
            ioReady()));

    resetPerfCounters();
"""

    boot_replacement = """    Serial.print(
        "IO_READY_BOOT=");
    Serial.println(
        yesNo(
            ioReady()));

    Serial.print(
        "RTU_READY_BOOT=");
    Serial.println(
        yesNo(
            rtuReady));

    Serial.print(
        "RTU_SLAVE_ID_BOOT=");
    Serial.println(
        RTU_SLAVE_ID);

    resetPerfCounters();
"""

    text = replace_once(
        text,
        boot_anchor,
        boot_replacement,
        "P5_RTU_BOOT",
    )

    loop_anchor = """    // Modbus recibe prioridad en cada vuelta.
    JWPLC_ModbusTCP.task();

    serviceSerialCommands();
"""

    loop_replacement = """    // Comunicaciones reciben prioridad en cada vuelta.
    JWPLC_ModbusTCP.task();
    serviceRtu();

    serviceSerialCommands();
"""

    text = replace_once(
        text,
        loop_anchor,
        loop_replacement,
        "P5_RTU_LOOP",
    )

    ready_print_anchor = """        Serial.print(
            "FULL_RUNTIME_READY=");

        Serial.println(
            yesNo(
                fullRuntimeReady()));
    }
}
"""

    ready_print_replacement = """        Serial.print(
            "FULL_RUNTIME_READY=");

        Serial.println(
            yesNo(
                fullRuntimeReady()));

        Serial.print(
            "COMBINED_RUNTIME_READY=");

        Serial.println(
            yesNo(
                combinedRuntimeReady()));
    }
}
"""

    text = replace_once(
        text,
        ready_print_anchor,
        ready_print_replacement,
        "P5_COMBINED_READY_ANNOUNCE",
    )

    path.write_text(text, encoding="utf-8", newline="\n")

    verify = path.read_text(encoding="utf-8")

    required = (
        "#include <JWPLC_ModbusRTU.h>",
        "RTU_SLAVE_ID = 2",
        "RTU_BAUD = 115200UL",
        "JWPLC_ModbusRTU.begin(",
        "JWPLC_ModbusRTU.task();",
        "ETH_READY=",
        "ETH_LINK=",
        "ETH_IP=",
        "RTU_RX_FRAMES=",
        "RTU_REQUESTS_OK=",
        "RTU_SERVICE_GAP_MAX_US=",
        "COMBINED_RUNTIME_READY=",
    )

    missing = [
        marker
        for marker in required
        if marker not in verify
    ]

    if missing:
        raise RuntimeError(
            "P5_POSTCONDITION_MISSING=" +
            ",".join(missing)
        )

    print("P5_COMBINED_RUNTIME_RTU_SLAVE_ID=2")
    print("P5_COMBINED_RUNTIME_RTU_BAUD=115200")
    print("P5_COMBINED_RUNTIME_RTU_CONFIG=8N1")
    print("P5_COMBINED_RUNTIME_PRODUCT_SOURCE_MUTATION=NO")
    print("P5_COMBINED_RUNTIME_RTU_PATCH=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
