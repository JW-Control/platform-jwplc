/*
  19_alpha6_dhcp_t1_t2_gate.ino

  Gate dedicado de Alpha6 para validar mantenimiento DHCP cooperativo.

  Requiere:
    - JWPLC Basic conectado por RJ45 a un router con DHCP.
    - No requiere Internet.
    - No desconectar RJ45 ni pulsar RESET durante la prueba.

  Comandos Serial 115200:
    T = ejecutar gate T1/T2.
    ? = ayuda.

  El build_opt.h de este sketch habilita hooks internos de prueba.
  Los hooks no existen en builds normales del package.
*/

#include <Arduino.h>
#include <JWPLC_Ethernet.h>

#if !defined(JWPLC_BASIC)
#error "Este sketch debe compilarse para JWPLC Basic (jwplcbasic)."
#endif

#if !defined(JWPLC_ETHERNET_ENABLE_TEST_HOOKS)
#error "Falta JWPLC_ETHERNET_ENABLE_TEST_HOOKS. Revisar build_opt.h."
#endif

static constexpr uint32_t SERIAL_BAUD = 115200UL;
static constexpr uint32_t PHASE_TIMEOUT_MS = 12000UL;
static constexpr uint32_t OTHER_TIMER_SEC = 60UL;
static constexpr uint32_t SERVICE_HARD_LIMIT_US = 100000UL;

static constexpr uint8_t TEST_MODE_RENEW = 1;
static constexpr uint8_t TEST_MODE_REBIND = 2;

static bool ipValid(IPAddress ip)
{
    return !(ip == IPAddress(0, 0, 0, 0)) &&
           !(ip == IPAddress(255, 255, 255, 255));
}

static void printHelp()
{
    Serial.println();
    Serial.println(F("============================================================"));
    Serial.println(F(" ALPHA6 - DHCP T1/T2 COOPERATIVE GATE"));
    Serial.println(F("============================================================"));
    Serial.println(F("T = ejecutar prueba completa T1 renew + T2 rebind"));
    Serial.println(F("? = ayuda"));
    Serial.println();
    Serial.println(F("Conexion: JWPLC -> RJ45 -> router con DHCP."));
    Serial.println(F("No requiere Internet."));
}

static bool runMaintenancePhase(
    const char *gate,
    bool rebind,
    uint32_t &maxServiceUs)
{
    const uint32_t forcedRenew = rebind ? OTHER_TIMER_SEC : 0UL;
    const uint32_t forcedRebind = rebind ? 0UL : OTHER_TIMER_SEC;
    const uint8_t expectedMode = rebind ? TEST_MODE_REBIND : TEST_MODE_RENEW;

    uint32_t renewBefore = 0;
    uint32_t rebindBefore = 0;

    if (!Ethernet.testSetDhcpLeaseTimers(forcedRenew, forcedRebind))
    {
        Serial.print(gate);
        Serial.println(F("=FAIL_SET_TIMERS"));
        return false;
    }

    if (!Ethernet.testGetDhcpLeaseTimers(renewBefore, rebindBefore))
    {
        Serial.print(gate);
        Serial.println(F("=FAIL_GET_TIMERS"));
        return false;
    }

    Serial.println();
    Serial.print(F("DHCP_MAINT_PHASE="));
    Serial.println(rebind ? F("T2_REBIND") : F("T1_RENEW"));
    Serial.print(F("FORCED_RENEW_SEC="));
    Serial.println(renewBefore);
    Serial.print(F("FORCED_REBIND_SEC="));
    Serial.println(rebindBefore);

    const IPAddress initialIP = JWPLC_Ethernet.localIP();
    const IPAddress initialGateway = JWPLC_Ethernet.gatewayIP();

    Serial.print(F("INITIAL_IP="));
    Serial.println(initialIP);
    Serial.print(F("INITIAL_GATEWAY="));
    Serial.println(initialGateway);

    maxServiceUs = 0;

    bool sawPending = false;
    bool sawDhc = false;
    bool readyStayedValid = true;
    bool ipStayedValid = true;
    bool linkStayedOn = true;

    // Primera llamada: debe seleccionar inequívocamente RENEW o REBIND.
    uint32_t t0 = micros();
    JWPLC_Ethernet.service();
    uint32_t serviceUs = micros() - t0;
    if (serviceUs > maxServiceUs)
        maxServiceUs = serviceUs;

    const uint8_t startedMode = Ethernet.testDhcpLeaseMaintenanceMode();
    const bool modePass = startedMode == expectedMode;

    Serial.print(F("STARTED_MODE="));
    Serial.println(startedMode);
    Serial.print(F("EXPECTED_MODE="));
    Serial.println(expectedMode);
    Serial.println(modePass ? F("DHCP_MAINT_MODE=PASS") : F("DHCP_MAINT_MODE=FAIL"));

    if (Ethernet.dhcpMaintenanceInProgress())
        sawPending = true;

    if (strcmp(JWPLC_Ethernet.diagnosticCode(), "DHC") == 0)
        sawDhc = true;

    const uint32_t started = millis();

    while ((uint32_t)(millis() - started) < PHASE_TIMEOUT_MS)
    {
        t0 = micros();
        JWPLC_Ethernet.service();
        serviceUs = micros() - t0;

        if (serviceUs > maxServiceUs)
            maxServiceUs = serviceUs;

        const bool pending = Ethernet.dhcpMaintenanceInProgress();

        if (pending)
            sawPending = true;

        if (strcmp(JWPLC_Ethernet.diagnosticCode(), "DHC") == 0)
            sawDhc = true;

        if (!JWPLC_Ethernet.isReady())
            readyStayedValid = false;

        if (!ipValid(JWPLC_Ethernet.localIP()))
            ipStayedValid = false;

        if (JWPLC_Ethernet.linkStatus() != LinkON)
            linkStayedOn = false;

        if (sawPending && !pending)
        {
            const bool finalReady = JWPLC_Ethernet.isReady();
            const bool finalIP = ipValid(JWPLC_Ethernet.localIP());
            const bool finalGateway = ipValid(JWPLC_Ethernet.gatewayIP());
            const bool finalLink = JWPLC_Ethernet.linkStatus() == LinkON;
            const bool finalError = JWPLC_Ethernet.lastError() == JWPLC_ETH_OK;
            const bool latencyPass = maxServiceUs <= SERVICE_HARD_LIMIT_US;

            uint32_t renewAfter = 0;
            uint32_t rebindAfter = 0;
            const bool timersReadable = Ethernet.testGetDhcpLeaseTimers(
                renewAfter,
                rebindAfter);

            Serial.print(F("SAW_PENDING="));
            Serial.println(sawPending ? F("YES") : F("NO"));
            Serial.print(F("SAW_DHC="));
            Serial.println(sawDhc ? F("YES") : F("NO"));
            Serial.print(F("READY_STAYED_VALID="));
            Serial.println(readyStayedValid ? F("YES") : F("NO"));
            Serial.print(F("IP_STAYED_VALID="));
            Serial.println(ipStayedValid ? F("YES") : F("NO"));
            Serial.print(F("LINK_STAYED_ON="));
            Serial.println(linkStayedOn ? F("YES") : F("NO"));
            Serial.print(F("MAX_SERVICE_US="));
            Serial.println(maxServiceUs);
            Serial.print(F("FINAL_IP="));
            Serial.println(JWPLC_Ethernet.localIP());
            Serial.print(F("FINAL_GATEWAY="));
            Serial.println(JWPLC_Ethernet.gatewayIP());

            if (timersReadable)
            {
                Serial.print(F("LEASE_RENEW_SEC="));
                Serial.println(renewAfter);
                Serial.print(F("LEASE_REBIND_SEC="));
                Serial.println(rebindAfter);
            }

            const bool pass =
                modePass &&
                sawPending &&
                sawDhc &&
                readyStayedValid &&
                ipStayedValid &&
                linkStayedOn &&
                finalReady &&
                finalIP &&
                finalGateway &&
                finalLink &&
                finalError &&
                latencyPass &&
                timersReadable;

            Serial.print(gate);
            Serial.println(pass ? F("=PASS") : F("=FAIL"));
            return pass;
        }

        if (sawPending &&
            !pending &&
            JWPLC_Ethernet.lastError() == JWPLC_ETH_DHCP_FAILED)
        {
            Serial.print(gate);
            Serial.println(F("=FAIL_DHCP"));
            return false;
        }

        delay(20);
    }

    Serial.print(F("SAW_PENDING="));
    Serial.println(sawPending ? F("YES") : F("NO"));
    Serial.print(F("SAW_DHC="));
    Serial.println(sawDhc ? F("YES") : F("NO"));
    Serial.print(F("MAX_SERVICE_US="));
    Serial.println(maxServiceUs);
    Serial.print(gate);
    Serial.println(F("=FAIL_TIMEOUT"));
    return false;
}

static void runGate()
{
    Serial.println();
    Serial.println(F("############################################################"));
    Serial.println(F(" ALPHA6 - DHCP T1/T2 COOPERATIVE GATE"));
    Serial.println(F("############################################################"));
    Serial.println(F("Mantener RJ45 conectado al router durante toda la prueba."));

    JWPLC_Ethernet.configure();
    JWPLC_Ethernet.useDefaultMac();
    JWPLC_Ethernet.useDHCP();
    JWPLC_Ethernet.setTimeouts(5000, 1000);
    JWPLC_Ethernet.setRetransmissionCount(3);

    Serial.println(F("Adquiriendo lease DHCP inicial..."));

    const uint32_t beginT0 = millis();
    const bool beginOk = JWPLC_Ethernet.begin();
    const uint32_t beginMs = millis() - beginT0;

    Serial.print(F("DHCP_INITIAL_BEGIN_MS="));
    Serial.println(beginMs);
    JWPLC_Ethernet.printStatus(Serial);

    const bool initialPass =
        beginOk &&
        JWPLC_Ethernet.isReady() &&
        JWPLC_Ethernet.linkStatus() == LinkON &&
        ipValid(JWPLC_Ethernet.localIP()) &&
        ipValid(JWPLC_Ethernet.gatewayIP());

    Serial.println(initialPass
        ? F("DHCP_INITIAL_LEASE=PASS")
        : F("DHCP_INITIAL_LEASE=FAIL"));

    if (!initialPass)
    {
        Serial.println(F("ALPHA6_DHCP_T1_T2=FAIL_INITIAL_LEASE"));
        return;
    }

    uint32_t t1MaxServiceUs = 0;
    uint32_t t2MaxServiceUs = 0;

    const bool t1Pass = runMaintenancePhase(
        "DHCP_T1_RENEW",
        false,
        t1MaxServiceUs);

    if (!t1Pass)
    {
        Serial.println(F("DHCP_T2_REBIND=NOT_RUN"));
        Serial.println(F("ALPHA6_DHCP_T1_T2=FAIL_T1"));
        return;
    }

    delay(250);

    const bool t2Pass = runMaintenancePhase(
        "DHCP_T2_REBIND",
        true,
        t2MaxServiceUs);

    const uint32_t overallMaxServiceUs =
        t1MaxServiceUs > t2MaxServiceUs
            ? t1MaxServiceUs
            : t2MaxServiceUs;

    const bool servicePass = overallMaxServiceUs <= SERVICE_HARD_LIMIT_US;

    Serial.println();
    Serial.print(F("T1_MAX_SERVICE_US="));
    Serial.println(t1MaxServiceUs);
    Serial.print(F("T2_MAX_SERVICE_US="));
    Serial.println(t2MaxServiceUs);
    Serial.print(F("OVERALL_MAX_SERVICE_US="));
    Serial.println(overallMaxServiceUs);
    Serial.println(servicePass
        ? F("DHCP_SERVICE_NONBLOCKING=PASS")
        : F("DHCP_SERVICE_NONBLOCKING=FAIL"));

    const bool finalPass =
        t1Pass &&
        t2Pass &&
        servicePass &&
        JWPLC_Ethernet.isReady() &&
        JWPLC_Ethernet.linkStatus() == LinkON &&
        ipValid(JWPLC_Ethernet.localIP());

    Serial.println(finalPass
        ? F("ALPHA6_DHCP_T1_T2=PASS")
        : F("ALPHA6_DHCP_T1_T2=FAIL"));

    JWPLC_Ethernet.printStatus(Serial);
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(800);
    printHelp();
    Serial.println(F("DHCP_T1_T2_GATE_BOOT=PASS"));
}

void loop()
{
    if (Serial.available() > 0)
    {
        const char cmd = (char)toupper((unsigned char)Serial.read());

        switch (cmd)
        {
        case 'T':
            runGate();
            break;

        case '?':
            printHelp();
            break;

        case '\r':
        case '\n':
        case ' ':
            break;

        default:
            Serial.println(F("Comandos: T=DHCP T1/T2, ?=ayuda"));
            break;
        }
    }

    delay(5);
}
