/*
  JWPLC_PCB_Acceptance_Test_v3_Alpha6_EthernetPilot.ino

  Test guiado de aceptacion de PCB para JWPLC Basic.
  Base de trabajo: v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime.

  Orden del wizard:

  I2C
    1. RTC DS3232M + avance + NVRAM backup/RW/restore.
    2. TCA6424A entradas: espera hasta observar I0_0..I0_7 activadas al menos una vez.
    3. TCA6424A salidas: walking Q0_0..Q0_7 + confirmacion auditiva de reles.

  Interfaz local
    4. Botonera: espera hasta detectar los 6 botones.
    5. Buzzer: 3 tonos + confirmacion manual.

  SPI
    6. FRAM: obligatoria. Backup -> RW -> restore. Un fallo es FAIL de PCB.
    7. microSD: prueba adaptativa segun disponibilidad de tarjeta y pin DETECT.
    8. TFT: patron visual + confirmacion manual.
    9. Ethernet/W5500:
       - W5500 SPI siempre obligatorio.
       - RJ45 opcional, declarado por el operador.
       - Sin RJ45: valida W5500 + LINK OFF esperado.
       - Con router: valida W5500 + LINK + DHCP/IP/gateway.
       - Con laptop directa sin DHCP: valida que Ethernet falle sin bloquear TCA/RTC/SPI.
       - Con laptop directa estatica: IP estatica + HTTP/TCP real contra la laptop.
   10. Mutex SPI adaptativo: solo usa perifericos realmente disponibles.
   11. Stress SPI de 10 minutos, con prioridad de muestreo Ethernet.

  Criterios adaptativos:
    - FRAM, RTC, TCA, botonera, buzzer, TFT y W5500 son obligatorios.
    - microSD puede omitirse si el operador no dispone de tarjeta; en ese caso
      se valida que el pin DETECT reporte ausencia.
    - RJ45 puede omitirse; en ese caso se valida W5500 por SPI y LINK OFF.
    - Si hay RJ45, el test de red se vuelve obligatorio para la ruta escogida.

  Seguridad:
    - Ejecutar TCA salidas con cargas/actuadores reales desconectados.
    - El test activa fisicamente Q0_0..Q0_7.
    - Las pruebas FRAM y SD restauran/limpian sus zonas temporales.

  Serial: 115200 baud.

  En espera final:
    P = imprimir resumen
    R = repetir wizard completo
    0 = apagar todas las salidas
*/

#include <Arduino.h>
#include <SPI.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_Display.h>

#include <ctype.h>
#include <string.h>

extern "C"
{
#include "jwplc_peripherals.h"
#include "jwplc_spi_bus.h"
}

#if !defined(JWPLC_BASIC)
#error "Este sketch debe compilarse para JWPLC Basic (jwplcbasic)."
#endif

// ============================================================================
// Configuracion
// ============================================================================

static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint8_t BUZZER_PIN = 26;

static constexpr uint8_t FRAM_PATTERN_BYTES = 32;
static constexpr uint8_t SD_PATTERN_BYTES = 128;
static constexpr uint16_t FRAM_TEST_CYCLES = 32;
static constexpr uint8_t SD_TEST_CYCLES = 8;

static constexpr uint32_t SPI_MUTEX_TIMEOUT_MS = 50;
static constexpr uint32_t SPI_MUTEX_REVIEW_US = 10000UL;
static constexpr uint32_t SPI_MUTEX_HARD_LIMIT_US = 50000UL;

static constexpr uint32_t SPI_MUTEX_GATE_MS = 60000UL;
static constexpr uint32_t SPI_LONG_STRESS_MS = 10UL * 60UL * 1000UL;

static constexpr uint16_t ETH_W5500_PROBE_SAMPLES = 32;
static constexpr uint32_t ETH_LINK_WAIT_MS = 15000UL;
static constexpr uint32_t ETH_HTTP_WAIT_MS = 120000UL;
static constexpr uint16_t ETH_HTTP_PORT = 8080;
static constexpr uint32_t ETH_NO_DHCP_OBSERVE_MS = 12000UL;
static constexpr uint32_t ETH_NO_DHCP_IO_MAX_AGE_MS = 250UL;
static constexpr uint32_t ETH_NO_DHCP_RTC_MAX_AGE_MS = 2500UL;

static const char SD_TEST_PATH[] = "/JWPCBTEST.TMP";

static const IPAddress LAPTOP_JWPLC_IP(192, 168, 77, 2);
static const IPAddress LAPTOP_PC_IP(192, 168, 77, 1);
static const IPAddress LAPTOP_SUBNET(255, 255, 255, 0);

// ============================================================================
// Tipos - todos declarados antes de la primera funcion para evitar problemas
// con el generador automatico de prototipos de Arduino.
// ============================================================================

enum TestResult : uint8_t
{
    TEST_NOT_RUN = 0,
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP,
    TEST_REVIEW,
    TEST_ABORTED
};

enum EthernetPath : uint8_t
{
    ETH_PATH_NOT_SET = 0,
    ETH_PATH_NO_CABLE,
    ETH_PATH_ROUTER_DHCP,
    ETH_PATH_LAPTOP_NO_DHCP,
    ETH_PATH_LAPTOP_STATIC
};

struct AcceptanceState
{
    TestResult rtc = TEST_NOT_RUN;
    TestResult tcaRuntime = TEST_NOT_RUN;
    TestResult tcaInputs = TEST_NOT_RUN;
    TestResult tcaOutputsShadow = TEST_NOT_RUN;
    TestResult tcaOutputsManual = TEST_NOT_RUN;
    TestResult buttons = TEST_NOT_RUN;
    TestResult buzzer = TEST_NOT_RUN;

    TestResult fram = TEST_NOT_RUN;
    TestResult sdDetect = TEST_NOT_RUN;
    TestResult sdRW = TEST_NOT_RUN;
    TestResult tftReady = TEST_NOT_RUN;
    TestResult tftVisual = TEST_NOT_RUN;

    TestResult ethW5500 = TEST_NOT_RUN;
    TestResult ethLink = TEST_NOT_RUN;
    TestResult ethNetwork = TEST_NOT_RUN;

    TestResult spiMutex = TEST_NOT_RUN;
    TestResult spiLongStress = TEST_NOT_RUN;

    bool sdCardAvailable = false;
    bool sdActiveForStress = false;

    bool ethCableExpected = false;
    bool ethActiveForStress = false;
    bool ethNetworkServerStarted = false;
    EthernetPath ethPath = ETH_PATH_NOT_SET;

    uint8_t outputManualFailMask = 0;
};

struct SPIStressStats
{
    uint32_t mutexSamples = 0;
    uint32_t mutexAcquireFails = 0;
    uint32_t mutexOver1ms = 0;
    uint32_t mutexOver10ms = 0;
    uint32_t mutexOver25ms = 0;
    uint32_t mutexMaxWaitUs = 0;

    uint32_t ethHwSamples = 0;
    uint32_t ethHwFails = 0;
    uint32_t ethLinkSamples = 0;
    uint32_t ethLinkDrops = 0;
    uint32_t ethUnexpectedLinkOn = 0;
    uint32_t ethBusLockTimeouts = 0;
    uint32_t ethDhcpMaintainFails = 0;
    uint32_t ethHttpRequests = 0;
    uint32_t ethHttpLockFails = 0;
    uint32_t ethMaxProbeUs = 0;

    uint32_t framReads = 0;
    uint32_t framFails = 0;
    uint32_t framMaxUs = 0;

    uint32_t sdReads = 0;
    uint32_t sdFails = 0;
    uint32_t sdMaxUs = 0;

    uint32_t tftFramesStart = 0;
    uint32_t tftFramesEnd = 0;

    bool aborted = false;
};

static AcceptanceState results;
static EthernetServer diagnosticServer(ETH_HTTP_PORT);

// TFT USER callbacks / estado.
static volatile bool displayTestActive = false;
static volatile bool displayStressActive = false;
static volatile uint32_t displayFrameCounter = 0;
static volatile uint16_t displayStressStep = 0;

// ============================================================================
// Helpers generales
// ============================================================================

static void printRule()
{
    Serial.println(F("------------------------------------------------------------"));
}

static void printStage(const char *title)
{
    Serial.println();
    Serial.println(F("============================================================"));
    Serial.println(title);
    Serial.println(F("============================================================"));
}

static const char *resultName(TestResult r)
{
    switch (r)
    {
    case TEST_PASS:
        return "PASS";
    case TEST_FAIL:
        return "FAIL";
    case TEST_SKIP:
        return "SKIP";
    case TEST_REVIEW:
        return "REVIEW";
    case TEST_ABORTED:
        return "ABORTED";
    default:
        return "PENDIENTE";
    }
}

static void printResult(const char *name, TestResult r)
{
    Serial.print(name);
    Serial.print(F("="));
    Serial.println(resultName(r));
}

static void updateMaxUs(uint32_t value, uint32_t &currentMax)
{
    if (value > currentMax)
        currentMax = value;
}

static bool ipValid(IPAddress ip)
{
    return !(ip == IPAddress(0, 0, 0, 0)) &&
           !(ip == IPAddress(255, 255, 255, 255));
}

static const char *hardwareStatusName(EthernetHardwareStatus status)
{
    switch (status)
    {
    case EthernetNoHardware:
        return "NO_HARDWARE";
    case EthernetW5100:
        return "W5100";
    case EthernetW5200:
        return "W5200";
    case EthernetW5500:
        return "W5500";
    default:
        return "UNKNOWN";
    }
}

static const char *linkStatusName(EthernetLinkStatus status)
{
    switch (status)
    {
    case LinkON:
        return "ON";
    case LinkOFF:
        return "OFF";
    default:
        return "UNKNOWN";
    }
}

static void clearSerialInput()
{
    while (Serial.available() > 0)
        (void)Serial.read();
}

static char waitChoice(const char *validChars)
{
    while (true)
    {
        if (Serial.available() > 0)
        {
            char c = (char)Serial.read();
            c = (char)toupper((unsigned char)c);

            if (strchr(validChars, c) != nullptr)
            {
                while (Serial.available() > 0)
                {
                    char extra = (char)Serial.read();
                    if (extra == '\n')
                        break;
                }
                return c;
            }
        }
        delay(5);
    }
}

static bool askYesNo(const __FlashStringHelper *question)
{
    clearSerialInput();
    Serial.println();
    Serial.print(question);
    Serial.println(F(" [S/N]"));
    const char c = waitChoice("SN");
    Serial.print(F("Respuesta: "));
    Serial.println(c == 'S' ? F("SI") : F("NO"));
    return c == 'S';
}

static void waitEnter(const __FlashStringHelper *message)
{
    clearSerialInput();
    Serial.println();
    Serial.println(message);
    Serial.println(F("Presiona ENTER para continuar."));

    while (true)
    {
        if (Serial.available() > 0)
        {
            char c = (char)Serial.read();
            if (c == '\r' || c == '\n')
            {
                while (Serial.available() > 0)
                    (void)Serial.read();
                return;
            }
        }
        delay(5);
    }
}

static bool mandatoryPass(TestResult r)
{
    return r == TEST_PASS;
}

// ============================================================================
// Diagnostico Ethernet explicito
// ============================================================================

static void printEthernetErrorDiagnosis(JWPLCEthernetError error)
{
    Serial.println(F("--- Diagnostico Ethernet ---"));

    switch (error)
    {
    case JWPLC_ETH_OK:
        Serial.println(F("ETH_DIAG=OK"));
        break;

    case JWPLC_ETH_SPI_NOT_READY:
        Serial.println(F("ETH_DIAG=SPI_NOT_READY"));
        Serial.println(F("Revisar inicializacion SPI, SCK/MISO/MOSI y estado del bus compartido."));
        break;

    case JWPLC_ETH_NO_HARDWARE:
        Serial.println(F("ETH_DIAG=W5500_NOT_DETECTED"));
        Serial.println(F("Revisar alimentacion del W5500, soldadura, CS ETH, MISO/MOSI/SCK y continuidad."));
        break;

    case JWPLC_ETH_LINK_OFF:
        Serial.println(F("ETH_DIAG=LINK_OFF"));
        Serial.println(F("W5500 puede estar operativo. Revisar RJ45, magneticos, cable y puerto remoto."));
        break;

    case JWPLC_ETH_DHCP_FAILED:
        Serial.println(F("ETH_DIAG=DHCP_FAILED"));
        Serial.println(F("Si LINK esta ON, la capa fisica funciona; revisar servidor DHCP/red."));
        break;

    case JWPLC_ETH_INVALID_IP:
        Serial.println(F("ETH_DIAG=INVALID_IP"));
        Serial.println(F("Revisar configuracion IP/DHCP. El W5500 y el LINK pueden seguir operativos."));
        break;

    case JWPLC_ETH_BUS_LOCK_TIMEOUT:
        Serial.println(F("ETH_DIAG=SPI_MUTEX_TIMEOUT"));
        Serial.println(F("Revisar contencion SPI, periferico que retiene mutex o CS activo indebidamente."));
        break;

    default:
        Serial.println(F("ETH_DIAG=UNKNOWN_ERROR"));
        Serial.println(F("Repetir prueba y revisar status/lastError del W5500."));
        break;
    }
}

// ============================================================================
// TFT USER callbacks
// ============================================================================

extern "C" bool jwplcUserDisplayRefreshNeededCallback(const JWPLC_IOState *io,
                                                       const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
    return displayTestActive || displayStressActive;
}

extern "C" void jwplcUserDisplayEnterCallback(void)
{
    auto &tft = JWPLC_Display.tft();

    const int16_t w = tft.width();
    const int16_t h = tft.height();

    tft.fillScreen(ST77XX_BLACK);

    const uint16_t colors[] = {
        ST77XX_RED,
        ST77XX_GREEN,
        ST77XX_BLUE,
        ST77XX_CYAN,
        ST77XX_MAGENTA,
        ST77XX_YELLOW};

    const int16_t barW = w / 6;
    for (uint8_t i = 0; i < 6; ++i)
    {
        const int16_t x = i * barW;
        const int16_t bw = (i == 5) ? (w - x) : barW;
        tft.fillRect(x, 0, bw, h / 3, colors[i]);
    }

    for (int16_t x = 0; x < w; x += 8)
        tft.drawFastVLine(x, h / 3, h / 3,
                          (x / 8) & 1 ? ST77XX_WHITE : ST77XX_BLACK);

    for (int16_t y = h / 3; y < (2 * h) / 3; y += 8)
        tft.drawFastHLine(0, y, w,
                          (y / 8) & 1 ? ST77XX_WHITE : ST77XX_BLACK);

    tft.fillRect(0, (2 * h) / 3, w, h - (2 * h) / 3, ST77XX_BLACK);
    tft.drawRect(0, 0, w, h, ST77XX_WHITE);
    tft.drawRect(2, 2, w - 4, h - 4, ST77XX_CYAN);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setCursor(8, (2 * h) / 3 + 8);
    tft.print("JWPLC PCB TEST - TFT");
    tft.setCursor(8, (2 * h) / 3 + 22);
    tft.print("Color + lineas + refresh SPI");

    displayFrameCounter = 0;
    displayStressStep = 0;
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io,
                                                  const JWPLC_RTCState *rtc)
{
    (void)rtc;

    if (!displayTestActive && !displayStressActive)
        return;

    auto &tft = JWPLC_Display.tft();
    const int16_t w = tft.width();
    const int16_t h = tft.height();

    const int16_t y = h - 18;
    const int16_t markerW = 18;
    const int16_t usable = (w > markerW) ? (w - markerW) : 1;
    const int16_t x = (displayStressStep * 7U) % usable;

    tft.fillRect(0, y, w, 18, ST77XX_BLACK);
    tft.fillRect(x, y + 2, markerW, 10,
                 (displayFrameCounter & 1U) ? ST77XX_GREEN : ST77XX_CYAN);

    if (io)
    {
        tft.setTextSize(1);
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        tft.setCursor(2, y + 11);
        tft.print("I=");
        tft.print((uint8_t)io->di_logical_bank0, HEX);
        tft.print(" Q=");
        tft.print((uint8_t)io->do_bank1, HEX);
    }

    ++displayStressStep;
    ++displayFrameCounter;
}

extern "C" void jwplcUserDisplayExitCallback(void)
{
    displayTestActive = false;
    displayStressActive = false;
}

// ============================================================================
// 1. RTC / I2C
// ============================================================================

static bool testRTC()
{
    printStage("[1/11] I2C - RTC DS3232M");

    const bool present = JWPLC_RTC.isPresent();
    Serial.print(F("RTC present: "));
    Serial.println(present ? F("YES") : F("NO"));

    if (!present)
    {
        Serial.print(F("RTC error: "));
        Serial.println(JW_RTC::errorToString(JWPLC_RTC.lastError()));
        results.rtc = TEST_FAIL;
        printResult("RTC_I2C_NVRAM", results.rtc);
        return false;
    }

    JWRTCDateTime a = {};
    JWRTCDateTime b = {};

    const bool readA = JWPLC_RTC.read(a);
    delay(1100);
    const bool readB = JWPLC_RTC.read(b);

    const bool advanced = readA && readB &&
                          (a.second != b.second ||
                           a.minute != b.minute ||
                           a.hour != b.hour ||
                           a.day != b.day);

    Serial.print(F("Read A/B: "));
    Serial.print(readA ? F("OK") : F("FAIL"));
    Serial.print('/');
    Serial.println(readB ? F("OK") : F("FAIL"));

    if (readB)
    {
        Serial.print(F("RTC: "));
        Serial.print(b.year);
        Serial.print('-');
        Serial.print(b.month);
        Serial.print('-');
        Serial.print(b.day);
        Serial.print(' ');
        Serial.print(b.hour);
        Serial.print(':');
        Serial.print(b.minute);
        Serial.print(':');
        Serial.println(b.second);
    }

    Serial.print(F("Clock advanced: "));
    Serial.println(advanced ? F("YES") : F("NO"));
    Serial.print(F("Lost power flag: "));
    Serial.println(JWPLC_RTC.lostPower() ? F("YES") : F("NO"));

    constexpr uint8_t NVRAM_ADDR = JW_RTC::NVRAM_SIZE - 1;
    uint8_t backup = 0;
    uint8_t verify = 0;

    const bool nvRead = JWPLC_RTC.nvramReadByte(NVRAM_ADDR, backup);
    const uint8_t testValue = (uint8_t)(backup ^ 0xA5U);
    const bool nvWrite = nvRead && JWPLC_RTC.nvramWriteByte(NVRAM_ADDR, testValue);
    const bool nvVerifyRead = nvWrite && JWPLC_RTC.nvramReadByte(NVRAM_ADDR, verify);
    const bool nvVerify = nvVerifyRead && verify == testValue;

    bool nvRestore = false;
    if (nvRead)
    {
        nvRestore = JWPLC_RTC.nvramWriteByte(NVRAM_ADDR, backup);
        uint8_t restored = 0;
        nvRestore = nvRestore &&
                    JWPLC_RTC.nvramReadByte(NVRAM_ADDR, restored) &&
                    restored == backup;
    }

    Serial.print(F("RTC NVRAM R/W: "));
    Serial.println((nvRead && nvWrite && nvVerify) ? F("PASS") : F("FAIL"));
    Serial.print(F("RTC NVRAM restore: "));
    Serial.println(nvRestore ? F("PASS") : F("FAIL"));

    const bool pass = present && readA && readB && advanced && nvVerify && nvRestore;
    results.rtc = pass ? TEST_PASS : TEST_FAIL;
    printResult("RTC_I2C_NVRAM", results.rtc);
    return pass;
}

// ============================================================================
// 2. TCA6424A entradas / I2C
// ============================================================================

static bool testTCARuntime()
{
    const JWPLC_IOState *io = jwplcGetIOState();

    if (!io || !io->initialized)
    {
        Serial.println(F("TCA_RUNTIME=FAIL: estado runtime no inicializado."));
        results.tcaRuntime = TEST_FAIL;
        return false;
    }

    const uint32_t scanA = io->last_scan_ms;
    delay(100);
    const uint32_t scanB = io->last_scan_ms;

    const bool scanning = scanB != scanA;
    results.tcaRuntime = scanning ? TEST_PASS : TEST_FAIL;

    Serial.print(F("TCA last_scan_ms A/B: "));
    Serial.print(scanA);
    Serial.print('/');
    Serial.println(scanB);
    printResult("TCA_RUNTIME_I2C", results.tcaRuntime);
    return scanning;
}

static void printPendingInputs(uint8_t seenMask)
{
    Serial.print(F("Pendientes: "));
    bool first = true;
    for (uint8_t i = 0; i < 8; ++i)
    {
        if ((seenMask & (uint8_t)(1U << i)) == 0)
        {
            if (!first)
                Serial.print(F(", "));
            Serial.print(F("I0_"));
            Serial.print(i);
            first = false;
        }
    }
    if (first)
        Serial.print(F("NINGUNA"));
    Serial.println();
}

static bool testTCAInputsGuided()
{
    printStage("[2/11] I2C - TCA6424A ENTRADAS I0_0..I0_7");

    if (!testTCARuntime())
    {
        results.tcaInputs = TEST_FAIL;
        return false;
    }

    Serial.println(F("Puedes activar las entradas en cualquier orden."));
    Serial.println(F("Cada I0_x debe observarse activa al menos una vez."));

    waitEnter(F("Deja inicialmente TODAS las entradas desactivadas."));

    uint8_t bitmap = JWPLC_readInputs();
    uint8_t lastBitmap = 0xFF;

    while (bitmap != 0x00)
    {
        if (bitmap != lastBitmap)
        {
            lastBitmap = bitmap;
            Serial.print(F("Aun hay entradas activas: 0x"));
            if (bitmap < 0x10)
                Serial.print('0');
            Serial.println(bitmap, HEX);
            Serial.println(F("Desactivalas para fijar el baseline."));
        }

        const JWPLC_IOState *io = jwplcGetIOState();
        if (!io || (uint32_t)(millis() - io->last_scan_ms) > 1000UL)
        {
            Serial.println(F("TCA_INPUTS=FAIL: el scan I2C del TCA dejo de actualizarse."));
            results.tcaInputs = TEST_FAIL;
            return false;
        }

        delay(20);
        bitmap = JWPLC_readInputs();
    }

    Serial.println(F("Baseline 0x00 confirmado. Ahora activa las 8 entradas."));
    printPendingInputs(0);

    uint8_t seenMask = 0;
    lastBitmap = 0;

    while (seenMask != 0xFF)
    {
        bitmap = JWPLC_readInputs();

        if (bitmap != lastBitmap)
        {
            const uint8_t newlyActive = (uint8_t)(bitmap & (uint8_t)~seenMask);

            for (uint8_t i = 0; i < 8; ++i)
            {
                if (newlyActive & (uint8_t)(1U << i))
                {
                    Serial.print(F("[INPUT PASS] I0_"));
                    Serial.println(i);
                }
            }

            seenMask |= bitmap;

            Serial.print(F("Inputs bitmap=0x"));
            if (bitmap < 0x10)
                Serial.print('0');
            Serial.print(bitmap, HEX);
            Serial.print(F(" | seen=0x"));
            if (seenMask < 0x10)
                Serial.print('0');
            Serial.println(seenMask, HEX);
            printPendingInputs(seenMask);

            lastBitmap = bitmap;
        }

        const JWPLC_IOState *io = jwplcGetIOState();
        if (!io || (uint32_t)(millis() - io->last_scan_ms) > 1000UL)
        {
            Serial.println(F("TCA_INPUTS=FAIL: perdida de comunicacion/scan TCA durante la prueba."));
            results.tcaInputs = TEST_FAIL;
            return false;
        }

        delay(10);
    }

    results.tcaInputs = TEST_PASS;
    printResult("TCA_INPUTS_PHYSICAL", results.tcaInputs);

    Serial.println(F("Todas las entradas fueron observadas activas al menos una vez."));
    Serial.println(F("Desactiva todas las entradas para dejar la PCB en reposo."));

    const uint32_t t0 = millis();
    while (JWPLC_readInputs() != 0x00 && (uint32_t)(millis() - t0) < 15000UL)
        delay(20);

    return true;
}

// ============================================================================
// 3. TCA6424A salidas / I2C
// ============================================================================

static void pulseOutput(uint8_t index, uint8_t repeats = 1)
{
    for (uint8_t r = 0; r < repeats; ++r)
    {
        const uint8_t bitmap = (uint8_t)(1U << index);

        Serial.print(F("Q0_"));
        Serial.println(index);
        Serial.println(F("  -> ON"));
        JWPLC_writeOutputs(bitmap);
        delay(650);

        Serial.println(F("  -> OFF"));
        JWPLC_writeOutputs(0x00);
        delay(300);
    }
}

static bool testTCAOutputsGuided()
{
    printStage("[3/11] I2C - TCA6424A SALIDAS Q0_0..Q0_7");

    Serial.println(F("ADVERTENCIA: se activaran fisicamente los 8 reles."));
    waitEnter(F("Confirma que las cargas/actuadores reales estan desconectados."));

    JWPLC_writeOutputs(0x00);
    delay(250);

    bool shadowPass = true;

    for (uint8_t i = 0; i < 8; ++i)
    {
        const uint8_t bitmap = (uint8_t)(1U << i);

        Serial.println();
        Serial.print(F("[RELE Q0_"));
        Serial.print(i);
        Serial.println(F("] escucha ON y OFF"));

        JWPLC_writeOutputs(bitmap);
        delay(100);

        const uint8_t shadowOn = JWPLC_readOutputs();
        if (shadowOn != bitmap)
        {
            shadowPass = false;
            Serial.print(F("OUTPUT_SHADOW_FAIL Q0_"));
            Serial.print(i);
            Serial.print(F(" expected=0x"));
            Serial.print(bitmap, HEX);
            Serial.print(F(" got=0x"));
            Serial.println(shadowOn, HEX);
        }

        delay(550);
        JWPLC_writeOutputs(0x00);
        delay(100);

        const uint8_t shadowOff = JWPLC_readOutputs();
        if (shadowOff != 0)
        {
            shadowPass = false;
            Serial.print(F("OUTPUT_SHADOW_OFF_FAIL Q0_"));
            Serial.println(i);
        }

        delay(250);
    }

    JWPLC_writeOutputs(0x00);
    results.tcaOutputsShadow = shadowPass ? TEST_PASS : TEST_FAIL;
    printResult("TCA_OUTPUT_SHADOW", results.tcaOutputsShadow);

    const bool allSounded = askYesNo(F("Escuchaste claramente el ON/OFF de los 8 reles?"));

    if (allSounded)
    {
        results.tcaOutputsManual = TEST_PASS;
        printResult("TCA_OUTPUT_RELAYS_AUDIO", results.tcaOutputsManual);
        return shadowPass;
    }

    Serial.println(F("Se inicia localizacion individual. Responde S/N despues de cada rele."));
    results.outputManualFailMask = 0;

    for (uint8_t i = 0; i < 8; ++i)
    {
        Serial.println();
        Serial.print(F("Repeticion Q0_"));
        Serial.println(i);
        pulseOutput(i, 2);

        Serial.print(F("Q0_"));
        Serial.print(i);
        const bool heard = askYesNo(F(" produjo clicks ON/OFF correctos?"));
        if (!heard)
            results.outputManualFailMask |= (uint8_t)(1U << i);
    }

    if (results.outputManualFailMask == 0)
        results.tcaOutputsManual = TEST_PASS;
    else
        results.tcaOutputsManual = TEST_FAIL;

    Serial.print(F("OUTPUT_AUDIO_FAIL_MASK=0x"));
    if (results.outputManualFailMask < 0x10)
        Serial.print('0');
    Serial.println(results.outputManualFailMask, HEX);
    printResult("TCA_OUTPUT_RELAYS_AUDIO", results.tcaOutputsManual);

    return shadowPass && results.tcaOutputsManual == TEST_PASS;
}

// ============================================================================
// 4. Botonera
// ============================================================================

static const char *buttonName(uint8_t id)
{
    switch (id)
    {
    case BTN_LEFT:
        return "LEFT";
    case BTN_UP:
        return "UP";
    case BTN_RIGHT:
        return "RIGHT";
    case BTN_ESC:
        return "ESC";
    case BTN_OK:
        return "OK";
    case BTN_DOWN:
        return "DOWN";
    default:
        return "?";
    }
}

static void printPendingButtons(uint8_t seenMask)
{
    Serial.print(F("Botones pendientes: "));
    bool first = true;

    for (uint8_t id = 0; id < BTN_COUNT; ++id)
    {
        if ((seenMask & (uint8_t)(1U << id)) == 0)
        {
            if (!first)
                Serial.print(F(", "));
            Serial.print(buttonName(id));
            first = false;
        }
    }

    if (first)
        Serial.print(F("NINGUNO"));
    Serial.println();
}

static bool testButtonsGuided()
{
    printStage("[4/11] BOTONERA - 6 BOTONES");

    if (!JWPLCButtons::isReady())
    {
        Serial.println(F("BUTTONS=FAIL: JW_MatrixButtons no esta ready."));
        results.buttons = TEST_FAIL;
        return false;
    }

    JWPLCButtons::clearPendingInput();

    uint8_t seenMask = 0;
    bool previous[BTN_COUNT] = {};

    Serial.println(F("Pulsa LEFT, UP, RIGHT, ESC, OK y DOWN en cualquier orden."));
    printPendingButtons(seenMask);

    while (seenMask != ((1U << BTN_COUNT) - 1U))
    {
        for (uint8_t id = 0; id < BTN_COUNT; ++id)
        {
            const bool down = JWPLC_Buttons.isDown(id);

            if (down && !previous[id])
            {
                if ((seenMask & (uint8_t)(1U << id)) == 0)
                {
                    seenMask |= (uint8_t)(1U << id);
                    Serial.print(F("[BUTTON PASS] "));
                    Serial.println(buttonName(id));
                    printPendingButtons(seenMask);
                }
            }

            previous[id] = down;
        }

        delay(5);
    }

    results.buttons = TEST_PASS;
    printResult("BUTTONS_PHYSICAL", results.buttons);
    return true;
}

// ============================================================================
// 5. Buzzer
// ============================================================================

static bool testBuzzerGuided()
{
    printStage("[5/11] BUZZER - 3 TONOS");

    const uint16_t tonesHz[] = {880, 1320, 1760};

    for (uint8_t i = 0; i < 3; ++i)
    {
        tone(BUZZER_PIN, tonesHz[i], 220);
        delay(320);
    }
    noTone(BUZZER_PIN);

    const bool heard = askYesNo(F("Escuchaste correctamente los 3 tonos ascendentes?"));
    results.buzzer = heard ? TEST_PASS : TEST_FAIL;
    printResult("BUZZER_PHYSICAL", results.buzzer);
    return heard;
}

// ============================================================================
// 6. FRAM / SPI - obligatoria
// ============================================================================

static bool framProtectedWrite(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!JWPLC_FRAM.writeEnable(true))
        return false;

    const bool writeOk = JWPLC_FRAM.write(addr, data, len);
    const bool disableOk = JWPLC_FRAM.writeEnable(false);
    return writeOk && disableOk;
}

static bool testFRAM(uint16_t cycles = FRAM_TEST_CYCLES)
{
    printStage("[6/11] SPI - FRAM OBLIGATORIA");

    const uint32_t size = JWPLC_FRAM.size();
    Serial.print(F("FRAM size: "));
    Serial.println(size);

    if (size < FRAM_PATTERN_BYTES)
    {
        Serial.println(F("FRAM=FAIL: no inicializada / no responde / size invalido."));
        Serial.println(F("Al estar soldada, este fallo RECHAZA la PCB."));
        results.fram = TEST_FAIL;
        return false;
    }

    const uint32_t addr = size - FRAM_PATTERN_BYTES;
    uint8_t backup[FRAM_PATTERN_BYTES] = {};
    uint8_t pattern[FRAM_PATTERN_BYTES] = {};
    uint8_t readback[FRAM_PATTERN_BYTES] = {};

    if (!JWPLC_FRAM.read(addr, backup, sizeof(backup)))
    {
        Serial.println(F("FRAM=FAIL: fallo de lectura inicial/backup."));
        results.fram = TEST_FAIL;
        return false;
    }

    bool pass = true;
    uint32_t maxWriteUs = 0;
    uint32_t maxReadUs = 0;

    for (uint16_t c = 0; c < cycles; ++c)
    {
        for (uint8_t i = 0; i < sizeof(pattern); ++i)
            pattern[i] = (uint8_t)(0xA5U ^ i ^ c);

        const uint32_t w0 = micros();
        const bool writeOk = framProtectedWrite(addr, pattern, sizeof(pattern));
        const uint32_t writeUs = micros() - w0;
        updateMaxUs(writeUs, maxWriteUs);

        memset(readback, 0, sizeof(readback));
        const uint32_t r0 = micros();
        const bool readOk = JWPLC_FRAM.read(addr, readback, sizeof(readback));
        const uint32_t readUs = micros() - r0;
        updateMaxUs(readUs, maxReadUs);

        if (!writeOk || !readOk || memcmp(pattern, readback, sizeof(pattern)) != 0)
        {
            Serial.print(F("FRAM mismatch/fallo ciclo "));
            Serial.println(c);
            pass = false;
            break;
        }
    }

    const bool restoreWrite = framProtectedWrite(addr, backup, sizeof(backup));
    memset(readback, 0, sizeof(readback));
    const bool restoreRead = JWPLC_FRAM.read(addr, readback, sizeof(readback));
    const bool restoreMatch = restoreRead &&
                              memcmp(backup, readback, sizeof(backup)) == 0;

    if (!restoreWrite || !restoreMatch)
    {
        Serial.println(F("FRAM_RESTORE=FAIL"));
        pass = false;
    }
    else
    {
        Serial.println(F("FRAM_RESTORE=PASS"));
    }

    Serial.print(F("FRAM max write us: "));
    Serial.println(maxWriteUs);
    Serial.print(F("FRAM max read us: "));
    Serial.println(maxReadUs);

    results.fram = pass ? TEST_PASS : TEST_FAIL;
    printResult("FRAM_RW_STRESS", results.fram);

    if (!pass)
        Serial.println(F("FRAM_CRITICAL_FAILURE=PCB_REJECT"));

    return pass;
}

// ============================================================================
// 7. microSD / SPI - adaptativa + DETECT
// ============================================================================

static bool testSDReadWrite(uint8_t cycles = SD_TEST_CYCLES)
{
    if (!JWPLC_SD.isReady())
    {
        if (!JWPLC_SD.begin())
        {
            Serial.print(F("SD begin failed: "));
            Serial.println(JWPLC_SD.lastErrorString());
            return false;
        }
    }

    Serial.print(F("SD card type: "));
    Serial.println(JWPLC_SD.cardType());
    Serial.print(F("SD card size bytes: "));
    Serial.println((unsigned long long)JWPLC_SD.cardSize());

    uint8_t tx[SD_PATTERN_BYTES] = {};
    uint8_t rx[SD_PATTERN_BYTES] = {};
    bool pass = true;
    uint32_t maxCycleUs = 0;

    if (JWPLC_SD.exists(SD_TEST_PATH))
    {
        if (!JWPLC_SD.remove(SD_TEST_PATH))
        {
            Serial.println(F("SD=FAIL: no se pudo limpiar archivo temporal previo."));
            return false;
        }
    }

    for (uint8_t c = 0; c < cycles; ++c)
    {
        for (uint16_t i = 0; i < sizeof(tx); ++i)
            tx[i] = (uint8_t)(0x5AU ^ i ^ (c * 13U));

        const uint32_t t0 = micros();

        JWPLCFile f = JWPLC_SD.open(SD_TEST_PATH, FILE_WRITE);
        if (!f)
        {
            Serial.print(F("SD open write FAIL ciclo "));
            Serial.println(c);
            pass = false;
            break;
        }

        const size_t written = f.write(tx, sizeof(tx));
        f.flush();
        f.close();

        if (written != sizeof(tx))
        {
            Serial.println(F("SD short write"));
            pass = false;
            break;
        }

        memset(rx, 0, sizeof(rx));
        JWPLCFile r = JWPLC_SD.open(SD_TEST_PATH, FILE_READ);
        if (!r)
        {
            Serial.println(F("SD open read FAIL"));
            pass = false;
            break;
        }

        size_t n = 0;
        while (r.available() && n < sizeof(rx))
        {
            const int v = r.read();
            if (v < 0)
                break;
            rx[n++] = (uint8_t)v;
        }
        r.close();

        const uint32_t cycleUs = micros() - t0;
        updateMaxUs(cycleUs, maxCycleUs);

        if (n != sizeof(rx) || memcmp(tx, rx, sizeof(tx)) != 0)
        {
            Serial.print(F("SD mismatch ciclo "));
            Serial.print(c);
            Serial.print(F(" bytes="));
            Serial.println(n);
            pass = false;
            break;
        }

        if (!JWPLC_SD.remove(SD_TEST_PATH))
        {
            Serial.println(F("SD remove FAIL"));
            pass = false;
            break;
        }
    }

    if (JWPLC_SD.exists(SD_TEST_PATH))
        (void)JWPLC_SD.remove(SD_TEST_PATH);

    Serial.print(F("SD max cycle us: "));
    Serial.println(maxCycleUs);
    return pass;
}

static bool testSDGuided()
{
    printStage("[7/11] SPI - microSD + PIN DETECT");

    if (!JWPLCSD::isEnabled())
    {
        Serial.println(F("SD=FAIL: el perfil JWPLC Basic reporta SD deshabilitada."));
        results.sdDetect = TEST_FAIL;
        results.sdRW = TEST_FAIL;
        return false;
    }

    results.sdCardAvailable = askYesNo(F("Tienes una microSD disponible para esta prueba?"));

    if (!results.sdCardAvailable)
    {
        waitEnter(F("Asegurate de que NO haya microSD insertada."));

        const bool detected = JWPLCSD::isCardPresent();
        Serial.print(F("SD detect pin reports present: "));
        Serial.println(detected ? F("YES") : F("NO"));

        if (detected)
        {
            Serial.println(F("FALLO DE DETECCION DE microSD: pin indica PRESENTE sin tarjeta."));
            results.sdDetect = TEST_FAIL;
            results.sdRW = TEST_SKIP;
            results.sdActiveForStress = false;
            return false;
        }

        Serial.println(F("SD_DETECT_ABSENT=PASS"));
        Serial.println(F("SD_RW=SKIP_NO_CARD_AVAILABLE"));
        results.sdDetect = TEST_PASS;
        results.sdRW = TEST_SKIP;
        results.sdActiveForStress = false;
        return true;
    }

    // Con tarjeta disponible validamos ambos estados del pin DETECT.
    waitEnter(F("RETIRA la microSD para validar estado AUSENTE del pin DETECT."));

    if (JWPLCSD::isCardPresent())
    {
        Serial.println(F("FALLO DE DETECCION DE microSD: pin sigue indicando PRESENTE sin tarjeta."));
        results.sdDetect = TEST_FAIL;
        results.sdRW = TEST_NOT_RUN;
        results.sdActiveForStress = false;
        return false;
    }

    Serial.println(F("SD_DETECT_ABSENT=PASS"));

    waitEnter(F("INSERTA ahora una microSD conocida-buena."));

    const uint32_t detectStart = millis();
    while (!JWPLCSD::isCardPresent() &&
           (uint32_t)(millis() - detectStart) < 30000UL)
    {
        delay(50);
    }

    if (!JWPLCSD::isCardPresent())
    {
        Serial.println(F("FALLO DE DETECCION DE microSD: tarjeta insertada pero DETECT no se activa."));
        results.sdDetect = TEST_FAIL;
        results.sdRW = TEST_NOT_RUN;
        results.sdActiveForStress = false;
        return false;
    }

    Serial.println(F("SD_DETECT_PRESENT=PASS"));
    results.sdDetect = TEST_PASS;

    const bool rwPass = testSDReadWrite();
    results.sdRW = rwPass ? TEST_PASS : TEST_FAIL;
    results.sdActiveForStress = rwPass;
    printResult("SD_RW_STRESS", results.sdRW);

    if (!rwPass)
    {
        Serial.print(F("SD last error: "));
        Serial.println(JWPLC_SD.lastErrorString());
    }

    return rwPass;
}

// ============================================================================
// 8. TFT / SPI
// ============================================================================

static bool testTFTGuided()
{
    printStage("[8/11] SPI - TFT ST7789 VISUAL");

    if (!JWPLC_Display.isReady())
    {
        Serial.println(F("TFT=FAIL: JWPLC_Display no esta ready."));
        results.tftReady = TEST_FAIL;
        results.tftVisual = TEST_FAIL;
        return false;
    }

    results.tftReady = TEST_PASS;

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserRefreshPeriodMs(50);
    JWPLC_Display.goIdle();
    delay(50);

    displayTestActive = true;
    displayStressActive = false;
    displayFrameCounter = 0;

    JWPLC_Display.enterUserUI();
    JWPLC_Display.forceRedraw();

    Serial.println(F("Comprueba visualmente:"));
    Serial.println(F("- 6 barras de color correctas."));
    Serial.println(F("- borde completo, sin filas/columnas muertas."));
    Serial.println(F("- patron blanco/negro limpio."));
    Serial.println(F("- marcador inferior moviendose."));

    delay(1500);
    const bool visualOk = askYesNo(F("La TFT se ve correcta y el marcador se mueve?"));

    results.tftVisual = visualOk ? TEST_PASS : TEST_FAIL;
    printResult("TFT_VISUAL", results.tftVisual);

    displayTestActive = false;
    JWPLC_Display.goIdle();
    return visualOk;
}

// ============================================================================
// 9. Ethernet / W5500 / SPI + red opcional
// ============================================================================

static bool probeW5500Hardware()
{
    Serial.println(F("W5500: probe fisico rapido SIN DHCP."));

    JWPLC_Ethernet.configure();
    JWPLC_Ethernet.useDefaultMac();
    JWPLC_Ethernet.useDHCP();
    JWPLC_Ethernet.setTimeouts(1500, 500);
    JWPLC_Ethernet.setRetransmissionCount(2);

    const uint32_t probeStart = millis();
    const bool phyReady = JWPLC_Ethernet.probeHardware();
    const uint32_t probeMs = millis() - probeStart;

    Serial.print(F("probeHardware(): "));
    Serial.print(phyReady ? F("PHY_READY") : F("NO_PHY_READY"));
    Serial.print(F(" in "));
    Serial.print(probeMs);
    Serial.println(F(" ms"));
    Serial.print(F("Runtime status: "));
    Serial.println(JWPLC_Ethernet.statusString());
    Serial.print(F("Diagnostic code: "));
    Serial.println(JWPLC_Ethernet.diagnosticCode());

    uint16_t good = 0;
    uint16_t bad = 0;
    uint16_t lockTimeouts = 0;

    for (uint16_t i = 0; i < ETH_W5500_PROBE_SAMPLES; ++i)
    {
        const EthernetHardwareStatus hw = JWPLC_Ethernet.hardwareStatus();

        if (hw == EthernetW5500)
            ++good;
        else
            ++bad;

        if (JWPLC_Ethernet.lastError() == JWPLC_ETH_BUS_LOCK_TIMEOUT)
            ++lockTimeouts;

        delay(10);
    }

    Serial.print(F("W5500 samples good/bad: "));
    Serial.print(good);
    Serial.print('/');
    Serial.println(bad);
    Serial.print(F("W5500 SPI lock timeouts: "));
    Serial.println(lockTimeouts);

    // phyReady puede ser false si no hay RJ45. El criterio obligatorio de este
    // probe es detectar el W5500 de forma estable, no obtener red.
    const bool pass = good == ETH_W5500_PROBE_SAMPLES && bad == 0 && lockTimeouts == 0;
    results.ethW5500 = pass ? TEST_PASS : TEST_FAIL;
    results.ethActiveForStress = pass;
    printResult("ETH_W5500_SPI", results.ethW5500);

    if (!pass)
    {
        printEthernetErrorDiagnosis(JWPLC_Ethernet.lastError());
        Serial.println(F("ETH_W5500_CRITICAL_FAILURE=PCB_REJECT"));
    }

    return pass;
}

static bool waitEthernetLinkOn(uint32_t timeoutMs)
{
    const uint32_t started = millis();
    EthernetLinkStatus last = Unknown;

    while ((uint32_t)(millis() - started) < timeoutMs)
    {
        const EthernetLinkStatus link = JWPLC_Ethernet.linkStatus();

        if (link != last)
        {
            Serial.print(F("ETH LINK: "));
            Serial.println(linkStatusName(link));
            last = link;
        }

        if (link == LinkON)
            return true;

        if (JWPLC_Ethernet.lastError() == JWPLC_ETH_BUS_LOCK_TIMEOUT)
        {
            Serial.println(F("ETH LINK probe: SPI lock timeout."));
        }

        delay(250);
    }

    return false;
}

static bool startDiagnosticHttpServer()
{
    if (!jwplcSPI_acquire(200))
    {
        Serial.println(F("ETH_HTTP_SERVER=FAIL: no se pudo adquirir mutex SPI."));
        return false;
    }

    jwplcSPI_deselectAll();
    diagnosticServer.begin();
    jwplcSPI_release();

    results.ethNetworkServerStarted = true;
    Serial.print(F("ETH_HTTP_SERVER=READY port "));
    Serial.println(ETH_HTTP_PORT);
    return true;
}

static bool serviceDiagnosticHttpServerOnce(bool printRequest)
{
    if (!results.ethNetworkServerStarted)
        return false;

    if (!jwplcSPI_acquire(100))
        return false;

    jwplcSPI_deselectAll();
    EthernetClient client = diagnosticServer.available();
    bool received = false;

    if (client)
    {
        received = true;

        while (client.available() > 0)
            (void)client.read();

        client.println(F("HTTP/1.1 200 OK"));
        client.println(F("Content-Type: text/plain"));
        client.println(F("Connection: close"));
        client.println();
        client.println(F("JWPLC BASIC ETHERNET PASS"));
        client.stop();
    }

    jwplcSPI_release();

    if (received && printRequest)
        Serial.println(F("ETH_HTTP_REQUEST=RECEIVED_FROM_LAPTOP"));

    return received;
}

static bool testRouterDHCP()
{
    Serial.println();
    Serial.println(F("Ruta: ROUTER/SWITCH con DHCP. No se requiere Internet."));

    JWPLC_Ethernet.configure();
    JWPLC_Ethernet.useDefaultMac();
    JWPLC_Ethernet.useDHCP();
    JWPLC_Ethernet.setTimeouts(5000, 1000);
    JWPLC_Ethernet.setRetransmissionCount(3);

    const uint32_t t0 = millis();
    const bool beginOk = JWPLC_Ethernet.begin();
    const uint32_t elapsed = millis() - t0;

    Serial.print(F("DHCP begin(): "));
    Serial.print(beginOk ? F("true") : F("false"));
    Serial.print(F(" in "));
    Serial.print(elapsed);
    Serial.println(F(" ms"));

    JWPLC_Ethernet.printStatus(Serial);

    const IPAddress ip = JWPLC_Ethernet.localIP();
    const IPAddress gateway = JWPLC_Ethernet.gatewayIP();
    const bool pass = beginOk &&
                      JWPLC_Ethernet.linkStatus() == LinkON &&
                      ipValid(ip) &&
                      ipValid(gateway);

    if (!pass)
        printEthernetErrorDiagnosis(JWPLC_Ethernet.lastError());

    Serial.println(pass ? F("ETH_ROUTER_DHCP=PASS") : F("ETH_ROUTER_DHCP=FAIL"));

    // Arrancamos servidor tambien: durante stress puede recibir trafico opcional.
    if (pass)
        (void)startDiagnosticHttpServer();

    return pass;
}

static bool testLaptopNoDhcpNonBlocking()
{
    Serial.println();
    Serial.println(F("Ruta: LAPTOP DIRECTA + LINK ON + SIN SERVIDOR DHCP."));
    Serial.println(F("Objetivo: DHCP debe fallar/reintentar SIN congelar TCA, RTC ni SPI."));
    Serial.println(F("En la laptop NO habilites Internet Connection Sharing ni servidor DHCP."));
    Serial.println(F("Puedes dejar su Ethernet con IP estatica 192.168.77.1/24."));
    waitEnter(F("Deja el RJ45 conectado directamente y continua."));

    JWPLC_Ethernet.configure();
    JWPLC_Ethernet.useDefaultMac();
    JWPLC_Ethernet.useDHCP();
    JWPLC_Ethernet.setTimeouts(5000, 1000);
    JWPLC_Ethernet.setRetransmissionCount(3);

    // Fuerza un nuevo probe fisico limpio. El autoload continuara desde PHY_READY
    // usando service()/pollDHCP() de forma cooperativa.
    const bool phyReady = JWPLC_Ethernet.probeHardware();
    if (!phyReady || JWPLC_Ethernet.linkStatus() != LinkON)
    {
        Serial.println(F("ETH_NO_DHCP_NONBLOCKING=FAIL: LINK no esta ON."));
        return false;
    }

    const uint32_t started = millis();
    uint32_t maxIoAgeMs = 0;
    uint32_t maxRtcAgeMs = 0;
    uint32_t maxServiceUs = 0;
    uint32_t maxMutexWaitUs = 0;
    uint32_t mutexFails = 0;
    bool unexpectedReady = false;
    JWPLCEthernetRuntimeState lastState = (JWPLCEthernetRuntimeState)255;

    while ((uint32_t)(millis() - started) < ETH_NO_DHCP_OBSERVE_MS)
    {
        // service() es seguro de llamar tambien desde el sketch: cada llamada debe
        // realizar un unico paso corto. El autoload del core lo llama en paralelo.
        const uint32_t serviceT0 = micros();
        JWPLC_Ethernet.service();
        const uint32_t serviceUs = micros() - serviceT0;
        if (serviceUs > maxServiceUs)
            maxServiceUs = serviceUs;

        const JWPLCEthernetRuntimeState state = JWPLC_Ethernet.runtimeState();
        if (state != lastState)
        {
            Serial.print(F("ETH runtime state="));
            Serial.print((uint8_t)state);
            Serial.print(F(" code="));
            Serial.print(JWPLC_Ethernet.diagnosticCode());
            Serial.print(F(" status="));
            Serial.println(JWPLC_Ethernet.statusString());
            lastState = state;
        }

        if (JWPLC_Ethernet.isReady())
            unexpectedReady = true;

        const JWPLC_IOState *io = jwplcGetIOState();
        if (io)
        {
            const uint32_t age = millis() - io->last_scan_ms;
            if (age > maxIoAgeMs)
                maxIoAgeMs = age;
        }

        const JWPLC_RTCState *rtc = jwplcGetRTCState();
        if (rtc && rtc->valid)
        {
            const uint32_t age = millis() - rtc->last_update_ms;
            if (age > maxRtcAgeMs)
                maxRtcAgeMs = age;
        }

        const uint32_t mutexT0 = micros();
        if (jwplcSPI_acquire(SPI_MUTEX_TIMEOUT_MS))
        {
            const uint32_t waitUs = micros() - mutexT0;
            if (waitUs > maxMutexWaitUs)
                maxMutexWaitUs = waitUs;
            jwplcSPI_release();
        }
        else
        {
            ++mutexFails;
        }

        delay(20);
    }

    Serial.println();
    Serial.println(F("ETH_NO_DHCP_METRICS:"));
    Serial.print(F("  max service() us : "));
    Serial.println(maxServiceUs);
    Serial.print(F("  max TCA age ms   : "));
    Serial.println(maxIoAgeMs);
    Serial.print(F("  max RTC age ms   : "));
    Serial.println(maxRtcAgeMs);
    Serial.print(F("  max mutex wait us: "));
    Serial.println(maxMutexWaitUs);
    Serial.print(F("  mutex fails      : "));
    Serial.println(mutexFails);
    Serial.print(F("  final code       : "));
    Serial.println(JWPLC_Ethernet.diagnosticCode());

    const bool ioPass = maxIoAgeMs <= ETH_NO_DHCP_IO_MAX_AGE_MS;
    const bool rtcPass = maxRtcAgeMs <= ETH_NO_DHCP_RTC_MAX_AGE_MS;
    const bool spiPass = mutexFails == 0 &&
                         maxMutexWaitUs <= SPI_MUTEX_HARD_LIMIT_US;
    const bool expectedNetworkFailure = !unexpectedReady &&
                                        JWPLC_Ethernet.localIP() == IPAddress(0, 0, 0, 0);

    Serial.println(ioPass ? F("ETH_NO_DHCP_TCA=PASS") : F("ETH_NO_DHCP_TCA=FAIL"));
    Serial.println(rtcPass ? F("ETH_NO_DHCP_RTC=PASS") : F("ETH_NO_DHCP_RTC=FAIL"));
    Serial.println(spiPass ? F("ETH_NO_DHCP_SPI=PASS") : F("ETH_NO_DHCP_SPI=FAIL"));
    Serial.println(expectedNetworkFailure
                       ? F("ETH_NO_DHCP_EXPECTED_NETWORK_FAILURE=PASS")
                       : F("ETH_NO_DHCP_EXPECTED_NETWORK_FAILURE=FAIL"));

    const bool pass = phyReady && ioPass && rtcPass && spiPass && expectedNetworkFailure;
    Serial.println(pass ? F("ETH_NO_DHCP_NONBLOCKING=PASS")
                        : F("ETH_NO_DHCP_NONBLOCKING=FAIL"));
    return pass;
}

static bool testLaptopStatic()
{
    Serial.println();
    Serial.println(F("Ruta: LAPTOP DIRECTA, sin Internet ni DHCP."));
    Serial.println(F("Configura el adaptador Ethernet de la laptop asi:"));
    Serial.println(F("  IPv4 laptop : 192.168.77.1"));
    Serial.println(F("  Mascara     : 255.255.255.0"));
    Serial.println(F("  Gateway/DNS : puede quedar vacio para esta prueba"));
    Serial.println(F("JWPLC usara 192.168.77.2:8080"));

    waitEnter(F("Configura la laptop y deja el cable RJ45 conectado directamente."));

    JWPLC_Ethernet.configure();
    JWPLC_Ethernet.useDefaultMac();
    JWPLC_Ethernet.setStaticIP(
        LAPTOP_JWPLC_IP,
        LAPTOP_PC_IP,
        LAPTOP_PC_IP,
        LAPTOP_SUBNET);
    JWPLC_Ethernet.setTimeouts(1500, 500);
    JWPLC_Ethernet.setRetransmissionCount(2);

    const bool beginOk = JWPLC_Ethernet.begin();

    JWPLC_Ethernet.printStatus(Serial);

    if (!beginOk || JWPLC_Ethernet.linkStatus() != LinkON ||
        JWPLC_Ethernet.localIP() != LAPTOP_JWPLC_IP)
    {
        Serial.println(F("ETH_LAPTOP_STATIC=FAIL"));
        printEthernetErrorDiagnosis(JWPLC_Ethernet.lastError());
        return false;
    }

    if (!startDiagnosticHttpServer())
        return false;

    Serial.println();
    Serial.println(F("En la laptop abre en el navegador:"));
    Serial.println(F("  http://192.168.77.2:8080/"));
    Serial.println(F("El JWPLC espera hasta 120 s una conexion HTTP real."));

    const uint32_t started = millis();
    while ((uint32_t)(millis() - started) < ETH_HTTP_WAIT_MS)
    {
        if (serviceDiagnosticHttpServerOnce(true))
        {
            Serial.println(F("ETH_LAPTOP_TCP_HTTP=PASS"));
            return true;
        }

        if (JWPLC_Ethernet.linkStatus() != LinkON)
        {
            Serial.println(F("ETH_LAPTOP_TCP_HTTP=FAIL: LINK se perdio durante espera."));
            return false;
        }

        delay(20);
    }

    Serial.println(F("ETH_LAPTOP_TCP_HTTP=FAIL_TIMEOUT"));
    Serial.println(F("W5500/IP pueden estar bien; revisar IP de laptop, firewall, cable y navegador."));
    return false;
}

static bool testEthernetGuided()
{
    printStage("[9/11] SPI - ETHERNET W5500 / RJ45 / RED");

    const bool w5500Pass = probeW5500Hardware();

    results.ethCableExpected = askYesNo(F("Hay un cable RJ45 conectado para probar LINK/red?"));

    const EthernetLinkStatus currentLink = JWPLC_Ethernet.linkStatus();
    Serial.print(F("Link observado ahora: "));
    Serial.println(linkStatusName(currentLink));

    if (!results.ethCableExpected)
    {
        results.ethPath = ETH_PATH_NO_CABLE;
        results.ethNetwork = TEST_SKIP;

        if (currentLink == LinkOFF)
        {
            results.ethLink = TEST_PASS;
            Serial.println(F("ETH_LINK_NO_CABLE=PASS (LINK OFF esperado)"));
        }
        else if (currentLink == LinkON)
        {
            results.ethLink = TEST_FAIL;
            Serial.println(F("ETH_LINK_NO_CABLE=FAIL: operador indico sin cable pero LINK esta ON."));
        }
        else
        {
            results.ethLink = TEST_REVIEW;
            Serial.println(F("ETH_LINK_NO_CABLE=REVIEW: estado UNKNOWN."));
        }

        Serial.println(F("ETH_NETWORK=SKIP_NO_RJ45"));
        Serial.println(F("Para stress SPI se seguira usando W5500 por registros SPI, sin sockets/DHCP."));
        return w5500Pass && results.ethLink == TEST_PASS;
    }

    Serial.println(F("Esperando LINK ON hasta 15 s..."));
    const bool linkOk = waitEthernetLinkOn(ETH_LINK_WAIT_MS);
    results.ethLink = linkOk ? TEST_PASS : TEST_FAIL;
    printResult("ETH_PHY_LINK", results.ethLink);

    if (!linkOk)
    {
        Serial.println(F("ETH_LINK_EXPECTED_BUT_DOWN=FAIL"));
        Serial.println(F("W5500 SPI puede haber pasado: revisar RJ45, magneticos, cable y puerto remoto."));
        printEthernetErrorDiagnosis(JWPLC_Ethernet.lastError());
        results.ethNetwork = TEST_FAIL;
        return false;
    }

    Serial.println();
    Serial.println(F("Selecciona el otro extremo del cable:"));
    Serial.println(F("  R = router/switch con DHCP"));
    Serial.println(F("  N = laptop directa, LINK ON y SIN DHCP (fail-safe no bloqueante)"));
    Serial.println(F("  L = laptop directa con IP estatica + HTTP"));
    const char path = waitChoice("RNL");

    bool networkPass = false;

    if (path == 'R')
    {
        results.ethPath = ETH_PATH_ROUTER_DHCP;
        networkPass = testRouterDHCP();
    }
    else if (path == 'N')
    {
        results.ethPath = ETH_PATH_LAPTOP_NO_DHCP;
        networkPass = testLaptopNoDhcpNonBlocking();
    }
    else
    {
        results.ethPath = ETH_PATH_LAPTOP_STATIC;
        networkPass = testLaptopStatic();
    }

    results.ethNetwork = networkPass ? TEST_PASS : TEST_FAIL;
    printResult("ETH_NETWORK_OR_FAILSAFE", results.ethNetwork);

    return w5500Pass && linkOk && networkPass;
}

// ============================================================================
// SPI mutex / stress adaptativo
// ============================================================================

static bool probeSPIMutex(SPIStressStats &stats)
{
    const uint32_t t0 = micros();
    const bool acquired = jwplcSPI_acquire(SPI_MUTEX_TIMEOUT_MS);
    const uint32_t waitUs = micros() - t0;

    ++stats.mutexSamples;
    updateMaxUs(waitUs, stats.mutexMaxWaitUs);

    if (waitUs > 1000UL)
        ++stats.mutexOver1ms;
    if (waitUs > SPI_MUTEX_REVIEW_US)
        ++stats.mutexOver10ms;
    if (waitUs > 25000UL)
        ++stats.mutexOver25ms;

    if (!acquired)
    {
        ++stats.mutexAcquireFails;
        return false;
    }

    jwplcSPI_deselectAll();
    jwplcSPI_release();
    return true;
}

static void stressEthernetProbe(SPIStressStats &stats)
{
    if (!results.ethActiveForStress)
        return;

    const uint32_t t0 = micros();
    const EthernetHardwareStatus hw = JWPLC_Ethernet.hardwareStatus();
    const uint32_t elapsed = micros() - t0;
    updateMaxUs(elapsed, stats.ethMaxProbeUs);

    ++stats.ethHwSamples;

    const JWPLCEthernetError hwError = JWPLC_Ethernet.lastError();

    if (hw != EthernetW5500)
    {
        ++stats.ethHwFails;

        // lastError() es historico/persistente: solo atribuir el timeout a
        // esta muestra cuando hardwareStatus() tambien reporto fallo.
        if (hwError == JWPLC_ETH_BUS_LOCK_TIMEOUT)
            ++stats.ethBusLockTimeouts;
    }

    const EthernetLinkStatus link = JWPLC_Ethernet.linkStatus();
    const JWPLCEthernetError linkError = JWPLC_Ethernet.lastError();
    ++stats.ethLinkSamples;

    // Unknown es el retorno de linkStatus() cuando no pudo consultar el W5500.
    // Solo entonces un BUS_LOCK_TIMEOUT puede atribuirse a esta operacion.
    if (link == Unknown && linkError == JWPLC_ETH_BUS_LOCK_TIMEOUT)
        ++stats.ethBusLockTimeouts;

    if (results.ethCableExpected)
    {
        if (link != LinkON)
            ++stats.ethLinkDrops;
    }
    else
    {
        if (link == LinkON)
            ++stats.ethUnexpectedLinkOn;
    }
}

static void stressFRAMRead(SPIStressStats &stats, uint32_t seed)
{
    if (results.fram != TEST_PASS)
        return;

    uint8_t value = 0;
    const uint32_t size = JWPLC_FRAM.size();
    const uint32_t addr = size ? (seed % size) : 0;

    const uint32_t t0 = micros();
    const bool ok = size > 0 && JWPLC_FRAM.read(addr, &value, 1);
    const uint32_t elapsed = micros() - t0;

    ++stats.framReads;
    updateMaxUs(elapsed, stats.framMaxUs);
    if (!ok)
        ++stats.framFails;
}

static void stressSDRead(SPIStressStats &stats)
{
    if (!results.sdActiveForStress)
        return;

    const uint32_t t0 = micros();
    const uint64_t size = JWPLC_SD.cardSize();
    const uint32_t elapsed = micros() - t0;

    ++stats.sdReads;
    updateMaxUs(elapsed, stats.sdMaxUs);

    if (size == 0 || !JWPLCSD::isCardPresent())
        ++stats.sdFails;
}

static bool serviceHttpForStress(SPIStressStats &stats)
{
    if (!results.ethNetworkServerStarted)
        return false;

    if (!jwplcSPI_acquire(50))
    {
        ++stats.ethHttpLockFails;
        return false;
    }

    jwplcSPI_deselectAll();
    EthernetClient client = diagnosticServer.available();
    bool received = false;

    if (client)
    {
        received = true;
        while (client.available() > 0)
            (void)client.read();

        client.println(F("HTTP/1.1 200 OK"));
        client.println(F("Content-Type: text/plain"));
        client.println(F("Connection: close"));
        client.println();
        client.println(F("JWPLC SPI STRESS OK"));
        client.stop();
    }

    jwplcSPI_release();

    if (received)
        ++stats.ethHttpRequests;

    return received;
}

static bool runAdaptiveSPIStress(uint32_t durationMs, bool longRun)
{
    printStage(longRun
                   ? "[11/11] SPI - STRESS FINAL 10 MINUTOS"
                   : "[10/11] SPI - MUTEX ADAPTATIVO");

    Serial.print(F("SPI ready: "));
    Serial.println(jwplcSPI_isReady() ? F("YES") : F("NO"));

    if (!jwplcSPI_isReady())
    {
        Serial.println(F("SPI_STRESS=FAIL: bus SPI no inicializado."));
        return false;
    }

    const bool useTFT = JWPLC_Display.isReady() && results.tftVisual == TEST_PASS;
    const bool useFRAM = results.fram == TEST_PASS;
    const bool useSD = results.sdActiveForStress;
    const bool useETH = results.ethActiveForStress;
    const bool requireEthLink = results.ethCableExpected;

    Serial.println(F("Perifericos incluidos:"));
    Serial.print(F("  TFT   : "));
    Serial.println(useTFT ? F("YES") : F("NO"));
    Serial.print(F("  FRAM  : "));
    Serial.println(useFRAM ? F("YES") : F("NO"));
    Serial.print(F("  microSD: "));
    Serial.println(useSD ? F("YES") : F("NO (omitida por resultado previo)"));
    Serial.print(F("  W5500 : "));
    Serial.println(useETH ? F("YES") : F("NO"));
    Serial.print(F("  ETH LINK requerido: "));
    Serial.println(requireEthLink ? F("YES") : F("NO"));

    // No tiene sentido declarar PASS de mutex/final si los SPI obligatorios no
    // llegaron sanos a esta etapa.
    if (!useTFT || !useFRAM || !useETH)
    {
        Serial.println(F("SPI_STRESS=FAIL_PRECONDITION"));
        Serial.println(F("Falta al menos uno de los SPI obligatorios: TFT/FRAM/W5500."));
        return false;
    }

    if (longRun)
    {
        Serial.println(F("Duracion minima: 10 minutos."));
        Serial.println(F("Se prioriza Ethernet: W5500 se consulta antes y despues de FRAM/SD."));

        if (results.ethNetworkServerStarted)
        {
            Serial.print(F("Servidor HTTP de stress disponible en puerto "));
            Serial.println(ETH_HTTP_PORT);
            Serial.println(F("Puedes generar trafico desde la laptop durante los 10 minutos."));
        }

        Serial.println(F("Envia X durante el stress para ABORTAR manualmente."));
    }
    else
    {
        Serial.println(F("Gate rapido de mutex: 60 s."));
    }

    SPIStressStats stats;

    displayTestActive = false;
    displayStressActive = true;
    displayFrameCounter = 0;
    JWPLC_Display.setUserRefreshPeriodMs(50);
    JWPLC_Display.enterUserUI();
    JWPLC_Display.forceRedraw();
    delay(100);

    stats.tftFramesStart = displayFrameCounter;

    const uint32_t started = millis();
    uint32_t lastProgress = started;

    bool dhcpFailureLatched = false;
    uint32_t cycle = 0;

    while ((uint32_t)(millis() - started) < durationMs)
    {
        if (longRun && Serial.available() > 0)
        {
            const char c = (char)toupper((unsigned char)Serial.read());
            if (c == 'X')
            {
                stats.aborted = true;
                Serial.println(F("SPI_STRESS_ABORTED_BY_OPERATOR"));
                break;
            }
        }

        // Ethernet tiene prioridad y aparece dos veces por ciclo.
        (void)probeSPIMutex(stats);
        stressEthernetProbe(stats);

        (void)probeSPIMutex(stats);
        stressFRAMRead(stats, cycle);

        stressEthernetProbe(stats);

        if (useSD)
        {
            (void)probeSPIMutex(stats);
            stressSDRead(stats);
        }

        if (results.ethNetworkServerStarted)
            (void)serviceHttpForStress(stats);

        (void)probeSPIMutex(stats);

        if ((cycle % 5U) == 0U)
            JWPLC_Display.forceRedraw();

        // El mantenimiento DHCP de router lo realiza exclusivamente
        // JWPLC_Ethernet.service() desde el runtime/autoload.
        // No llamar maintain() aqui: es la API sincrona legacy y podria
        // ocultar una regresion del mantenimiento cooperativo.
        const uint32_t now = millis();
        if (results.ethPath == ETH_PATH_ROUTER_DHCP)
        {
            const bool dhcpFailureNow =
                JWPLC_Ethernet.lastError() == JWPLC_ETH_DHCP_FAILED;

            // Cuenta una sola vez cada episodio de fallo del mantenimiento
            // DHCP cooperativo ejecutado por JWPLC_Ethernet.service().
            if (dhcpFailureNow && !dhcpFailureLatched)
            {
                ++stats.ethDhcpMaintainFails;
            }

            dhcpFailureLatched = dhcpFailureNow;
        }

        const uint32_t progressPeriod = longRun ? 30000UL : 10000UL;
        if ((uint32_t)(now - lastProgress) >= progressPeriod)
        {
            lastProgress = now;
            const uint32_t elapsedSec = (now - started) / 1000UL;

            Serial.print(F("[SPI STRESS] elapsed="));
            Serial.print(elapsedSec);
            Serial.print(F("s mutexFail="));
            Serial.print(stats.mutexAcquireFails);
            Serial.print(F(" ethHwFail="));
            Serial.print(stats.ethHwFails);
            Serial.print(F(" linkDrops="));
            Serial.print(stats.ethLinkDrops);
            Serial.print(F(" framFail="));
            Serial.print(stats.framFails);
            Serial.print(F(" sdFail="));
            Serial.print(stats.sdFails);
            Serial.print(F(" httpReq="));
            Serial.println(stats.ethHttpRequests);
        }

        ++cycle;
        delay(10);
    }

    stats.tftFramesEnd = displayFrameCounter;
    displayStressActive = false;
    JWPLC_Display.goIdle();

    Serial.println();
    Serial.println(F("--- SPI STRESS RESULTS ---"));
    Serial.print(F("Duration requested ms: "));
    Serial.println(durationMs);
    Serial.print(F("Cycles: "));
    Serial.println(cycle);

    Serial.println(F("--- MUTEX ---"));
    Serial.print(F("samples: "));
    Serial.println(stats.mutexSamples);
    Serial.print(F("acquire fails: "));
    Serial.println(stats.mutexAcquireFails);
    Serial.print(F("max wait us: "));
    Serial.println(stats.mutexMaxWaitUs);
    Serial.print(F(">1ms: "));
    Serial.println(stats.mutexOver1ms);
    Serial.print(F(">10ms: "));
    Serial.println(stats.mutexOver10ms);
    Serial.print(F(">25ms: "));
    Serial.println(stats.mutexOver25ms);

    Serial.println(F("--- ETHERNET / W5500 ---"));
    Serial.print(F("HW samples/fails: "));
    Serial.print(stats.ethHwSamples);
    Serial.print('/');
    Serial.println(stats.ethHwFails);
    Serial.print(F("Link samples/drops: "));
    Serial.print(stats.ethLinkSamples);
    Serial.print('/');
    Serial.println(stats.ethLinkDrops);
    Serial.print(F("Unexpected LINK ON (no cable): "));
    Serial.println(stats.ethUnexpectedLinkOn);
    Serial.print(F("ETH SPI lock timeouts: "));
    Serial.println(stats.ethBusLockTimeouts);
    Serial.print(F("DHCP maintain fails: "));
    Serial.println(stats.ethDhcpMaintainFails);
    Serial.print(F("HTTP requests during stress: "));
    Serial.println(stats.ethHttpRequests);
    Serial.print(F("HTTP lock fails: "));
    Serial.println(stats.ethHttpLockFails);
    Serial.print(F("ETH max probe us: "));
    Serial.println(stats.ethMaxProbeUs);

    Serial.println(F("--- FRAM ---"));
    Serial.print(F("reads/fails: "));
    Serial.print(stats.framReads);
    Serial.print('/');
    Serial.println(stats.framFails);
    Serial.print(F("max us: "));
    Serial.println(stats.framMaxUs);

    Serial.println(F("--- microSD ---"));
    Serial.print(F("reads/fails: "));
    Serial.print(stats.sdReads);
    Serial.print('/');
    Serial.println(stats.sdFails);
    Serial.print(F("max us: "));
    Serial.println(stats.sdMaxUs);

    Serial.println(F("--- TFT ---"));
    Serial.print(F("frames start/end: "));
    Serial.print(stats.tftFramesStart);
    Serial.print('/');
    Serial.println(stats.tftFramesEnd);

    const bool mutexSafety =
        stats.mutexSamples > 0 &&
        stats.mutexAcquireFails == 0 &&
        stats.mutexMaxWaitUs < SPI_MUTEX_HARD_LIMIT_US;

    const bool mutexLatency =
        mutexSafety && stats.mutexMaxWaitUs <= SPI_MUTEX_REVIEW_US;

    const bool tftPass = stats.tftFramesEnd > stats.tftFramesStart;
    const bool ethPass = stats.ethHwSamples > 0 &&
                         stats.ethHwFails == 0 &&
                         stats.ethBusLockTimeouts == 0 &&
                         stats.ethDhcpMaintainFails == 0 &&
                         (!requireEthLink || stats.ethLinkDrops == 0) &&
                         (requireEthLink || stats.ethUnexpectedLinkOn == 0);

    const bool framPass = stats.framReads > 0 && stats.framFails == 0;
    const bool sdPass = !useSD || (stats.sdReads > 0 && stats.sdFails == 0);

    const bool pass = !stats.aborted &&
                      mutexSafety &&
                      tftPass &&
                      ethPass &&
                      framPass &&
                      sdPass;

    printResult("SPI_MUTEX_TIMEOUT_SAFETY", mutexSafety ? TEST_PASS : TEST_FAIL);
    printResult("SPI_MUTEX_LATENCY",
                mutexLatency ? TEST_PASS : (mutexSafety ? TEST_REVIEW : TEST_FAIL));
    printResult("SPI_TFT_ACTIVITY", tftPass ? TEST_PASS : TEST_FAIL);
    printResult("SPI_ETH_W5500_STABILITY", ethPass ? TEST_PASS : TEST_FAIL);
    printResult("SPI_FRAM_STABILITY", framPass ? TEST_PASS : TEST_FAIL);
    printResult("SPI_SD_STABILITY", sdPass ? TEST_PASS : TEST_FAIL);

    if (mutexSafety && !mutexLatency)
    {
        Serial.println(F("SPI_MUTEX_LATENCY=REVIEW: hubo espera >10 ms sin timeout."));
        Serial.println(F("Investigar si es recurrente y correlacionar con periferico activo."));
    }

    if (!ethPass)
    {
        Serial.println(F("ETH_STRESS_FAILURE_DETAIL:"));
        if (stats.ethHwFails)
            Serial.println(F("- W5500 dejo de responder en una o mas muestras."));
        if (stats.ethBusLockTimeouts)
            Serial.println(F("- hubo timeouts del mutex desde Ethernet."));
        if (stats.ethLinkDrops)
            Serial.println(F("- LINK cayo durante stress cuando era requerido."));
        if (stats.ethDhcpMaintainFails)
            Serial.println(F("- DHCP renew/rebind fallo."));
    }

    if (longRun && stats.aborted)
    {
        results.spiLongStress = TEST_ABORTED;
        Serial.println(F("SPI_LONG_10MIN_STRESS=ABORTED"));
        Serial.println(F("SPI_LONG_10MIN_STRESS_PARTIAL_DATA=VALID_FOR_DIAGNOSTICS"));
        return false;
    }

    printResult(longRun ? "SPI_LONG_10MIN_STRESS" : "SPI_ADAPTIVE_MUTEX_GATE",
                pass ? TEST_PASS : TEST_FAIL);

    return pass;
}

// ============================================================================
// Resumen final
// ============================================================================

static void printSummary()
{
    Serial.println();
    Serial.println(F("============================================================"));
    Serial.println(F("JWPLC BASIC - PCB ACCEPTANCE SUMMARY v3 / ALPHA6 ETH PILOT"));
    Serial.println(F("============================================================"));

    Serial.println(F("I2C:"));
    printResult("RTC_I2C_NVRAM", results.rtc);
    printResult("TCA_RUNTIME_I2C", results.tcaRuntime);
    printResult("TCA_INPUTS_PHYSICAL", results.tcaInputs);
    printResult("TCA_OUTPUT_SHADOW", results.tcaOutputsShadow);
    printResult("TCA_OUTPUT_RELAYS_AUDIO", results.tcaOutputsManual);

    Serial.println(F("INTERFAZ:"));
    printResult("BUTTONS_PHYSICAL", results.buttons);
    printResult("BUZZER_PHYSICAL", results.buzzer);

    Serial.println(F("SPI:"));
    printResult("FRAM_RW_STRESS", results.fram);
    printResult("SD_DETECT", results.sdDetect);
    printResult("SD_RW", results.sdRW);
    printResult("TFT_READY", results.tftReady);
    printResult("TFT_VISUAL", results.tftVisual);

    Serial.println(F("ETHERNET:"));
    printResult("ETH_W5500_SPI", results.ethW5500);
    printResult("ETH_PHY_LINK", results.ethLink);
    printResult("ETH_NETWORK_OR_FAILSAFE", results.ethNetwork);

    Serial.println(F("COEXISTENCIA SPI:"));
    printResult("SPI_ADAPTIVE_MUTEX_GATE", results.spiMutex);
    printResult("SPI_LONG_10MIN_STRESS", results.spiLongStress);

    const bool sdSatisfied =
        results.sdDetect == TEST_PASS &&
        (results.sdRW == TEST_PASS || results.sdRW == TEST_SKIP);

    const bool ethernetSatisfied =
        results.ethW5500 == TEST_PASS &&
        results.ethLink == TEST_PASS &&
        (!results.ethCableExpected || results.ethNetwork == TEST_PASS);

    const bool technicalBasePass =
        mandatoryPass(results.rtc) &&
        mandatoryPass(results.tcaRuntime) &&
        mandatoryPass(results.tcaInputs) &&
        mandatoryPass(results.tcaOutputsShadow) &&
        mandatoryPass(results.tcaOutputsManual) &&
        mandatoryPass(results.buttons) &&
        mandatoryPass(results.buzzer) &&
        mandatoryPass(results.fram) &&
        sdSatisfied &&
        mandatoryPass(results.tftReady) &&
        mandatoryPass(results.tftVisual) &&
        ethernetSatisfied &&
        mandatoryPass(results.spiMutex);

    const bool finalPass =
        technicalBasePass &&
        mandatoryPass(results.spiLongStress);

    const bool finalIncomplete =
        technicalBasePass &&
        (results.spiLongStress == TEST_ABORTED ||
         results.spiLongStress == TEST_NOT_RUN);

    Serial.println();
    Serial.print(F("PCB_FINAL_ACCEPTANCE="));

    if (finalPass)
        Serial.println(F("PASS"));
    else if (finalIncomplete)
        Serial.println(F("INCOMPLETE"));
    else
        Serial.println(F("FAIL"));

    if (finalPass)
    {
        Serial.println(F("PCB lista para siguiente etapa de aceptacion/barnizado."));
    }
    else if (finalIncomplete)
    {
        Serial.println(F("Acceptance incompleta: completar el stress SPI final de 10 minutos."));
        Serial.println(F("Los datos parciales no se contabilizan como fallo tecnico."));
    }
    else
    {
        Serial.println(F("NO barnizar: revisar los FAIL/REVIEW anteriores y repetir."));
    }

    Serial.println(F("============================================================"));
}

// ============================================================================
// Wizard principal
// ============================================================================

static void resetResults()
{
    results = AcceptanceState();
    JWPLC_writeOutputs(0x00);
    displayTestActive = false;
    displayStressActive = false;
}

static void runGuidedAcceptance()
{
    resetResults();

    Serial.println();
    Serial.println(F("############################################################"));
    Serial.println(F(" JWPLC BASIC - GUIDED PCB ACCEPTANCE v3 / ALPHA6 ETH PILOT"));
    Serial.println(F("############################################################"));

    Serial.println(F("Orden: RTC -> TCA IN -> TCA OUT -> botones -> buzzer ->"));
    Serial.println(F("FRAM -> microSD -> TFT -> Ethernet -> mutex -> stress 10 min."));

    // No se detiene al primer FAIL para obtener diagnostico completo, excepto
    // que un stress posterior no tenga precondiciones minimas.
    (void)testRTC();
    (void)testTCAInputsGuided();
    (void)testTCAOutputsGuided();
    (void)testButtonsGuided();
    (void)testBuzzerGuided();
    (void)testFRAM();
    (void)testSDGuided();
    (void)testTFTGuided();
    (void)testEthernetGuided();

    const bool mutexPass = runAdaptiveSPIStress(SPI_MUTEX_GATE_MS, false);
    results.spiMutex = mutexPass ? TEST_PASS : TEST_FAIL;

    bool longPass = false;
    if (mutexPass)
    {
        const bool startLong = askYesNo(F("Iniciar ahora el stress SPI final de 10 minutos?"));

        if (startLong)
        {
            longPass = runAdaptiveSPIStress(SPI_LONG_STRESS_MS, true);

            // runAdaptiveSPIStress() fija TEST_ABORTED si el operador envio X.
            // No sobrescribir ese estado con TEST_FAIL.
            if (results.spiLongStress != TEST_ABORTED)
                results.spiLongStress = longPass ? TEST_PASS : TEST_FAIL;
        }
        else
        {
            Serial.println(F("SPI_LONG_10MIN_STRESS=NOT_EXECUTED"));
            results.spiLongStress = TEST_NOT_RUN;
        }
    }
    else
    {
        Serial.println(F("SPI_LONG_10MIN_STRESS=FAIL_PRECONDITION: mutex gate no paso."));
        results.spiLongStress = TEST_NOT_RUN;
    }

    JWPLC_writeOutputs(0x00);
    printSummary();

    Serial.println();
    Serial.println(F("Wizard terminado. P=resumen, R=repetir, 0=salidas OFF."));
}

// ============================================================================
// Arduino setup / loop
// ============================================================================

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(700);

    Serial.println();
    Serial.println(F("============================================================"));
    Serial.println(F(" JWPLC BASIC - PCB ACCEPTANCE TEST v3 / ALPHA6 ETH PILOT"));
    Serial.println(F(" Branch: v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime"));
    Serial.println(F("============================================================"));

    Serial.print(F("Chip: "));
    Serial.println(ESP.getChipModel());
    Serial.print(F("CPU MHz: "));
    Serial.println(ESP.getCpuFreqMHz());
    Serial.print(F("Flash bytes: "));
    Serial.println(ESP.getFlashChipSize());
    Serial.print(F("Free heap: "));
    Serial.println(ESP.getFreeHeap());

    Serial.println();
    Serial.println(F("SPI esperado: SCK=18 MISO=19 MOSI=23"));
    Serial.println(F("CS: TFT=33 ETH=5 SD=32 FRAM=13"));
    Serial.println(F("Buzzer: GPIO26"));

    JWPLC_writeOutputs(0x00);

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setErrLed(false);
    JWPLC_Display.setBusLedAuto(true);
    JWPLC_Display.setEthLedAuto(true);

    Serial.println();
    Serial.println(F("Antes de iniciar:"));
    Serial.println(F("- desconecta cargas reales de Q0_0..Q0_7;"));
    Serial.println(F("- prepara 24 V para activar I0_0..I0_7;"));
    Serial.println(F("- ten a mano microSD si deseas validar SD;"));
    Serial.println(F("- Ethernet: router DHCP, laptop sin DHCP o laptop estatica."));
    Serial.println();
    Serial.println(F("Envia S para INICIAR el wizard."));
}

void loop()
{
    if (Serial.available() > 0)
    {
        const char cmd = (char)toupper((unsigned char)Serial.read());

        switch (cmd)
        {
        case 'S':
        case 'R':
            runGuidedAcceptance();
            break;

        case 'P':
            printSummary();
            break;

        case '0':
            JWPLC_writeOutputs(0x00);
            Serial.println(F("OUTPUTS=ALL_OFF"));
            break;

        case '\r':
        case '\n':
        case ' ':
            break;

        default:
            Serial.println(F("Comandos: S/R=wizard, P=resumen, 0=salidas OFF"));
            break;
        }
    }

    delay(5);
}
