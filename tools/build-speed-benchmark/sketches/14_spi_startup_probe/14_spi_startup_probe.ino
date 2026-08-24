// JWPLC v2.1.0-alpha.5
// Probe dirigido de contencion SPI durante el arranque.
//
// Objetivo:
// - repetir la observacion que en Alpha4 permitio detectar la retencion
//   artificial del mutex durante la inicializacion Ethernet;
// - medir FRAM y microSD justo despues de retornar setup();
// - correlacionar las latencias con el estado no invasivo de Ethernet;
// - comparar arranque, estado estable y redraws del TFT.
//
// IMPORTANTE:
// No se llaman statusString(), hardwareStatus(), linkStatus() ni localIP()
// durante las muestras porque esas APIs adquieren SPI y contaminarian el
// propio experimento. Solo se consultan campos de estado que no toman el bus.

struct ProbeStats
{
    uint16_t samples;
    uint16_t framOk;
    uint16_t framFail;
    uint16_t sdOk;
    uint16_t sdFail;
    uint16_t ethLockTimeoutSeen;
    uint32_t framMaxUs;
    uint32_t sdMaxUs;
};

static ProbeStats startupStats = {};
static ProbeStats stableStats = {};
static ProbeStats redrawStats = {};

static uint32_t loopOriginMs = 0;
static bool loopOriginSet = false;
static bool probeDone = false;

static const uint8_t STARTUP_SAMPLES = 8;
static const uint8_t STABLE_SAMPLES = 10;
static const uint8_t REDRAW_SAMPLES = 10;

static void updateMax(uint32_t value, uint32_t &target)
{
    if (value > target)
    {
        target = value;
    }
}

static void printEthernetSnapshot()
{
    Serial.print("ETH attempted=");
    Serial.print(JWPLC_Ethernet.isBeginAttempted() ? "SI" : "NO");

    Serial.print(" ready=");
    Serial.print(JWPLC_Ethernet.isReady() ? "SI" : "NO");

    Serial.print(" error=");
    Serial.print(JWPLC_Ethernet.lastErrorString());
}

static bool probeFRAM(uint32_t &elapsedUs)
{
    uint8_t value = 0;

    const uint32_t startUs = micros();
    const bool ok = JWPLC_FRAM.read(0, &value, 1);
    elapsedUs = micros() - startUs;

    return ok;
}

static bool probeSD(uint32_t &elapsedUs)
{
    const bool readyBefore = JWPLC_SD.isReady();

    const uint32_t startUs = micros();
    const uint64_t sizeBytes = JWPLC_SD.cardSize();
    elapsedUs = micros() - startUs;

    return readyBefore && sizeBytes > 0;
}

static void runSample(const char *phase, uint8_t index, ProbeStats &stats)
{
    const uint32_t tMs = millis() - loopOriginMs;

    uint32_t framUs = 0;
    uint32_t sdUs = 0;

    const bool framOk = probeFRAM(framUs);
    const bool sdOk = probeSD(sdUs);

    ++stats.samples;

    if (framOk)
    {
        ++stats.framOk;
    }
    else
    {
        ++stats.framFail;
    }

    if (sdOk)
    {
        ++stats.sdOk;
    }
    else
    {
        ++stats.sdFail;
    }

    if (JWPLC_Ethernet.lastError() == JWPLC_ETH_BUS_LOCK_TIMEOUT)
    {
        ++stats.ethLockTimeoutSeen;
    }

    updateMax(framUs, stats.framMaxUs);
    updateMax(sdUs, stats.sdMaxUs);

    Serial.print("[");
    Serial.print(phase);
    Serial.print(" ");
    Serial.print(index + 1);
    Serial.print("] t=");
    Serial.print(tMs);
    Serial.print(" ms | ");

    printEthernetSnapshot();

    Serial.print(" | FRAM=");
    Serial.print(framOk ? "OK " : "FAIL ");
    Serial.print(framUs);
    Serial.print(" us");

    Serial.print(" | SD=");
    Serial.print(sdOk ? "OK " : "FAIL ");
    Serial.print(sdUs);
    Serial.print(" us");

    if (!sdOk)
    {
        Serial.print(" sdReady=");
        Serial.print(JWPLC_SD.isReady() ? "SI" : "NO");
        Serial.print(" sdError=");
        Serial.print(JWPLC_SD.lastErrorString());
    }

    Serial.println();
}

static void printStats(const char *name, const ProbeStats &stats)
{
    Serial.println();
    Serial.print("--- ");
    Serial.print(name);
    Serial.println(" ---");

    Serial.print("Samples: ");
    Serial.println(stats.samples);

    Serial.print("FRAM OK/FAIL: ");
    Serial.print(stats.framOk);
    Serial.print("/");
    Serial.println(stats.framFail);

    Serial.print("FRAM max us: ");
    Serial.println(stats.framMaxUs);

    Serial.print("SD OK/FAIL: ");
    Serial.print(stats.sdOk);
    Serial.print("/");
    Serial.println(stats.sdFail);

    Serial.print("SD max us: ");
    Serial.println(stats.sdMaxUs);

    Serial.print("ETH SPI lock timeout observado: ");
    Serial.println(stats.ethLockTimeoutSeen);
}

static bool statsPass(const ProbeStats &stats)
{
    return
        stats.samples > 0 &&
        stats.framFail == 0 &&
        stats.sdFail == 0 &&
        stats.ethLockTimeoutSeen == 0;
}

static void runProbe()
{
    Serial.println();
    Serial.println("============================================");
    Serial.println(" JWPLC ALPHA5 - SPI STARTUP CONTENTION PROBE");
    Serial.println("============================================");
    Serial.print("Display ready al entrar a loop: ");
    Serial.println(JWPLC_Display.isReady() ? "YES" : "NO");
    Serial.print("SD ready al entrar a loop: ");
    Serial.println(JWPLC_SD.isReady() ? "YES" : "NO");
    Serial.println();

    // Fase A: inmediatamente despues de setup().
    // No hay espera inicial intencional.
    for (uint8_t i = 0; i < STARTUP_SAMPLES; ++i)
    {
        runSample("START", i, startupStats);
        delay(20);
    }

    // Fase B: ventana estable.
    const uint32_t stableTarget = loopOriginMs + 1000;
    while ((int32_t)(millis() - stableTarget) < 0)
    {
        delay(1);
    }

    for (uint8_t i = 0; i < STABLE_SAMPLES; ++i)
    {
        runSample("STABLE", i, stableStats);
        delay(20);
    }

    // Fase C: solicitar redraws reales del TFT alternando RUN.
    // El cambio de estado fuerza refresh mediante el runtime normal.
    bool runLed = false;

    for (uint8_t i = 0; i < REDRAW_SAMPLES; ++i)
    {
        runLed = !runLed;
        JWPLC_Display.setRunLed(runLed);

        // Dar oportunidad al runtime de empezar el refresh antes de medir.
        delay(2);

        runSample("REDRAW", i, redrawStats);
        delay(20);
    }

    // Restaurar indicador RUN en OFF al terminar el probe.
    JWPLC_Display.setRunLed(false);

    printStats("STARTUP", startupStats);
    printStats("STABLE", stableStats);
    printStats("REDRAW", redrawStats);

    const bool pass =
        statsPass(startupStats) &&
        statsPass(stableStats) &&
        statsPass(redrawStats);

    Serial.println();
    Serial.print("SPI_STARTUP_PROBE=");
    Serial.println(pass ? "PASS" : "REVIEW");
    Serial.println("SPI_STARTUP_PROBE=DONE");
}

void setup()
{
    Serial.begin(115200);
    delay(50);

    Serial.println();
    Serial.println("Alpha5 probe preparado; setup() termina ahora.");
}

void loop()
{
    if (probeDone)
    {
        delay(1000);
        return;
    }

    if (!loopOriginSet)
    {
        loopOriginMs = millis();
        loopOriginSet = true;
    }

    probeDone = true;
    runProbe();
}
