#include <Arduino.h>
#include <JWPLC_Ethernet.h>
#include <jwplc_spi_bus.h>
#include <utility/w5100.h>

static const char kToken[] = "__G6_TOKEN__";
static constexpr uint16_t kDutPort = 5020;
static constexpr uint16_t kPcPort = 5021;
static constexpr size_t kBigWrite = 65536;
static EthernetUDP g_udp;
static bool g_ready = false;
static bool g_done = false;
static uint8_t g_rx[256];

static bool busTake() {
    if (!jwplcSPI_acquire(150)) return false;
    jwplcSPI_deselectAll();
    return true;
}
static void busRelease() { jwplcSPI_release(); }

static bool ipv4(const String &value, IPAddress &ip) {
    unsigned a, b, c, d;
    if (sscanf(value.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4 ||
        a > 255 || b > 255 || c > 255 || d > 255) return false;
    ip = IPAddress((uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d);
    return true;
}

static void emit(const char *key, long val) {
    Serial.print(key);
    Serial.print('=');
    Serial.println(val);
    Serial.flush();
}

static void runTx(IPAddress pc) {
    int errors = 0;
    size_t written = 0;
    int result = 0;
    uint8_t *buffer = (uint8_t *)malloc(kBigWrite);
    if (!buffer) ++errors;
    if (buffer) {
        for (size_t i = 0; i < kBigWrite; ++i)
            buffer[i] = (uint8_t)(i & 255);
        if (busTake()) {
            const int started = g_udp.beginPacket(pc, kPcPort);
            if (started == 1) {
                written = g_udp.write(buffer, kBigWrite);
                result = g_udp.endPacket();
            } else {
                ++errors;
            }
            busRelease();
        } else {
            ++errors;
        }
        free(buffer);
    }
    const size_t expected = (size_t)W5100.SSIZE;
    if (written != expected || result != 1) ++errors;
    emit("G6_TX_ACCEPTED_BYTES", (long)written);
    emit("G6_TX_EXPECTED_BYTES", (long)expected);
    emit("G6_TX_END_PACKET", result);
    emit("G6_TX_ERRORS", errors);
    Serial.print("G6_TX_DONE="); Serial.println(kToken);
    Serial.flush();
}

static int waitPacket(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while ((uint32_t)(millis() - start) < timeoutMs) {
        if (busTake()) {
            const int packet = g_udp.parsePacket();
            busRelease();
            if (packet > 0) return packet;
        }
        delay(2);
    }
    return 0;
}

static void runPartial() {
    int errors = 0;
    const int packet = waitPacket(5000);
    int readBytes = -1;
    int drainedParse = -1;
    int left = -1;
    if (busTake()) {
        if (packet == 256) {
            readBytes = g_udp.read(g_rx, 8);
            left = g_udp.available();
            drainedParse = g_udp.parsePacket();  // Alpha14: solo un drenado
        }
        busRelease();
    }
    if (packet != 256 || readBytes != 8 || left != 248 ||
        drainedParse != 0) ++errors;
    if (readBytes == 8) {
        for (int i = 0; i < 8; ++i) {
            if (g_rx[i] != (uint8_t)((i * 3 + 7) & 255)) {
                ++errors;
                break;
            }
        }
    }
    emit("G6_PARTIAL_PACKET", packet);
    emit("G6_PARTIAL_READ", readBytes);
    emit("G6_PARTIAL_REMAINING", left);
    emit("G6_PARTIAL_DRAIN_PARSE", drainedParse);
    emit("G6_PARTIAL_ERRORS", errors);
    Serial.print("G6_PARTIAL_DONE="); Serial.println(kToken);
    Serial.flush();
}

static void runNextAndReopen() {
    int errors = 0;
    const int packet = waitPacket(5000);
    int got = -1;
    if (busTake()) {
        if (packet == 16) got = g_udp.read(g_rx, 32768);
        busRelease();
    }
    if (packet != 16 || got != 16) ++errors;
    if (got == 16) {
        for (int i = 0; i < got; i++)
            if (g_rx[i] != (uint8_t)(i * 5 + 1)) { ++errors; break; }
    }

    int availableAfterStop = -1;
    size_t writeAfterStop = 1;
    int beginPacketAfterStop = 1;
    int reopened = 0;
    if (busTake()) {
        g_udp.stop();
        availableAfterStop = g_udp.available();
        const uint8_t byte = 0x55;
        writeAfterStop = g_udp.write(&byte, 1);
        beginPacketAfterStop = g_udp.beginPacket(IPAddress(192, 0, 2, 1), kPcPort);
        reopened = g_udp.begin(kDutPort);
        busRelease();
    }
    if (availableAfterStop != 0 || writeAfterStop != 0 ||
        beginPacketAfterStop != 0 || reopened != 1) ++errors;
    emit("G6_NEXT_PACKET", packet);
    emit("G6_NEXT_READ", got);
    emit("G6_STOP_AVAILABLE", availableAfterStop);
    emit("G6_STOP_WRITE", (long)writeAfterStop);
    emit("G6_STOP_BEGIN_PACKET", beginPacketAfterStop);
    emit("G6_REOPEN", reopened);
    emit("G6_FINAL_ERRORS", errors);
    Serial.print("G6_RESULT="); Serial.println(errors == 0 ? "PASS" : "FAIL");
    Serial.print("G6_DONE="); Serial.println(kToken);
    Serial.flush();
    g_done = true;
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(400);
    // Ethernet peripheral autoload is part of the normal JWPLC runtime.
}
void loop() {
    if (g_done) { delay(50); return; }
    if (!JWPLC_Ethernet.isReady()) { delay(100); return; }
    if (!g_ready && busTake()) {
        g_ready = g_udp.begin(kDutPort) == 1;
        busRelease();
    }
    if (!g_ready) { delay(100); return; }

    static uint32_t last = 0;
    if ((uint32_t)(millis() - last) > 450) {
        last = millis();
        Serial.print("G6_READY="); Serial.print(kToken);
        Serial.print(" IP="); Serial.println(JWPLC_Ethernet.localIP());
    }
    if (Serial.available() == 0) { delay(3); return; }
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    const String txCmd = String("G6_TX:") + kToken + " ";
    if (cmd.startsWith(txCmd)) {
        IPAddress pc;
        if (ipv4(cmd.substring(txCmd.length()), pc)) runTx(pc);
    } else if (cmd == String("G6_PARTIAL:") + kToken) {
        runPartial();
    } else if (cmd == String("G6_NEXT:") + kToken) {
        runNextAndReopen();
    }
}
