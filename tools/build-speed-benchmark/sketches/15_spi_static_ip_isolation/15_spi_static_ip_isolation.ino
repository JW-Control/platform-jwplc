// JWPLC v2.1.0-alpha.5
// Aislamiento de contencion SPI: enlace Ethernet activo SIN DHCP.
//
// Usa una IP TEST-NET-1 reservada para documentación (RFC 5737) para
// mantener Link ON sin depender de una dirección real de la LAN.
// El objetivo no es comunicar por IP, sino comparar el tiempo de retención
// SPI contra el probe DHCP con el mismo hardware y RJ45 conectado.

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

static ProbeStats stats = {};
static uint32_t loopOriginMs = 0;
static bool done = false;

static void updateMax(uint32_t value, uint32_t &target)
{
    if (value > target)
    {
        target = value;
    }
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

static void runSample(uint8_t index)
{
    const uint32_t tMs = millis() - loopOriginMs;

    uint32_t framUs = 0;
    uint32_t sdUs = 0;

    const bool framOk = probeFRAM(framUs);
    const bool sdOk = probeSD(sdUs);

    ++stats.samples;
    framOk ? ++stats.framOk : ++stats.framFail;
    sdOk ? ++stats.sdOk : ++stats.sdFail;

    if (JWPLC_Ethernet.lastError() == JWPLC_ETH_BUS_LOCK_TIMEOUT)
    {
        ++stats.ethLockTimeoutSeen;
    }

    updateMax(framUs, stats.framMaxUs);
    updateMax(sdUs, stats.sdMaxUs);

    Serial.print("[STATIC ");
    Serial.print(index + 1);
    Serial.print("] t=");
    Serial.print(tMs);
    Serial.print(" ms | ETH attempted=");
    Serial.print(JWPLC_Ethernet.isBeginAttempted() ? "SI" : "NO");
    Serial.print(" ready=");
    Serial.print(JWPLC_Ethernet.isReady() ? "SI" : "NO");
    Serial.print(" error=");
    Serial.print(JWPLC_Ethernet.lastErrorString());
    Serial.print(" | FRAM=");
    Serial.print(framOk ? "OK " : "FAIL ");
    Serial.print(framUs);
    Serial.print(" us | SD=");
    Serial.print(sdOk ? "OK " : "FAIL ");
    Serial.print(sdUs);
    Serial.println(" us");
}

void setup()
{
    Serial.begin(115200);
    delay(50);

    // TEST-NET-1: no debe enrutar tráfico real. Sólo evita la rama DHCP.
    JWPLC_Ethernet.setStaticIP(
        IPAddress(192, 0, 2, 2),
        IPAddress(192, 0, 2, 1),
        IPAddress(192, 0, 2, 1),
        IPAddress(255, 255, 255, 0));

    Serial.println();
    Serial.println("Alpha5 SPI isolation - STATIC TEST-NET-1");
    Serial.println("Mantener RJ45 conectado a enlace activo.");
}

void loop()
{
    if (done)
    {
        delay(1000);
        return;
    }

    done = true;
    loopOriginMs = millis();

    // El tick Ethernet automático ocurre alrededor de 1000 ms después de
    // setup(). Empezamos poco antes para capturar la transición completa.
    while ((int32_t)((millis() - loopOriginMs) - 900) < 0)
    {
        delay(1);
    }

    for (uint8_t i = 0; i < 16; ++i)
    {
        runSample(i);
        delay(20);
    }

    Serial.println();
    Serial.println("--- STATIC IP ISOLATION ---");
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

    const bool pass =
        stats.framFail == 0 &&
        stats.sdFail == 0 &&
        stats.ethLockTimeoutSeen == 0;

    Serial.print("SPI_STATIC_IP_ISOLATION=");
    Serial.println(pass ? "PASS" : "REVIEW");
    Serial.println("SPI_STATIC_IP_ISOLATION=DONE");
}
