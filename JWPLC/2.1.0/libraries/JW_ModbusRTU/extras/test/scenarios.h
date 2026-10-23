// Escenarios compartidos por la prueba de equivalencia.
// El incluyente define MB (objeto Modbus) y MB_CLASS (tipo para estaticos).
#include <functional>
#include <string>

uint64_t g_mockUs = 1000000ULL;
std::vector<MockPinEvent> g_pinEvents;

static size_t g_txMark = 0;
static std::function<std::vector<uint8_t>(const std::vector<uint8_t> &)> g_responder;

// Cada yield() avanza 100 us y, si el Master transmitio una trama nueva,
// entrega la respuesta del esclavo simulado.
void yield()
{
    if (g_uart.tx.size() > g_txMark && g_responder)
    {
        std::vector<uint8_t> req(g_uart.tx.begin() + g_txMark, g_uart.tx.end());
        g_txMark = g_uart.tx.size();
        std::vector<uint8_t> rsp = g_responder(req);
        for (uint8_t b : rsp) g_uart.rx.push_back(b);
    }
    mockAdvanceUs(100);
}

static std::string hex(const std::vector<uint8_t> &v, size_t from = 0)
{
    std::string s;
    char b[4];
    for (size_t i = from; i < v.size(); i++) { snprintf(b, sizeof(b), "%02X", v[i]); s += b; }
    return s;
}

static std::vector<uint8_t> frame(std::vector<uint8_t> payload)
{
    uint16_t crc = MB_CLASS::crc16(payload.data(), payload.size());
    payload.push_back(crc & 0xFF);
    payload.push_back(crc >> 8);
    return payload;
}

// Esclavo simulado con 32 registros y 32 coils.
static uint16_t g_peerRegs[32];
static uint8_t g_peerCoils[32];
static std::vector<uint8_t> peer(const std::vector<uint8_t> &req)
{
    if (req.size() < 4) return {};
    uint8_t id = req[0], fc = req[1];
    uint16_t a = (req[2] << 8) | req[3];
    uint16_t q = req.size() >= 6 ? ((req[4] << 8) | req[5]) : 0;
    switch (fc)
    {
    case 3: case 4:
    {
        std::vector<uint8_t> p = {id, fc, (uint8_t)(q * 2)};
        for (uint16_t i = 0; i < q; i++) { p.push_back(g_peerRegs[a + i] >> 8); p.push_back(g_peerRegs[a + i] & 0xFF); }
        return frame(p);
    }
    case 1: case 2:
    {
        uint8_t bytes = (q + 7) / 8;
        std::vector<uint8_t> p = {id, fc, bytes};
        for (uint8_t i = 0; i < bytes; i++)
        {
            uint8_t v = 0;
            for (uint8_t b = 0; b < 8 && i * 8 + b < q; b++) if (g_peerCoils[a + i * 8 + b]) v |= 1 << b;
            p.push_back(v);
        }
        return frame(p);
    }
    case 5: g_peerCoils[a] = req[4] == 0xFF; return frame(std::vector<uint8_t>(req.begin(), req.begin() + 6));
    case 6: g_peerRegs[a] = q; return frame(std::vector<uint8_t>(req.begin(), req.begin() + 6));
    case 15: case 16: return frame(std::vector<uint8_t>(req.begin(), req.begin() + 6));
    }
    return frame({id, (uint8_t)(fc | 0x80), 0x01});
}

static void report(const char *name, bool ok, int result, std::vector<uint16_t> data = {})
{
    printf("%-34s ok=%d res=%d tx=%s", name, ok ? 1 : 0, result, hex(g_uart.tx, 0).c_str());
    if (!data.empty())
    {
        printf(" data=");
        for (uint16_t d : data) printf("%04X,", d);
    }
    printf("\n");
    g_uart.tx.clear();
    g_txMark = 0;
    g_uart.rx.clear();
}

static void runSlaveFrame(const char *name, std::vector<uint8_t> req)
{
    for (uint8_t b : req) g_uart.rx.push_back(b);
    for (int i = 0; i < 300; i++) { MB.task(); mockAdvanceUs(100); }
    report(name, true, (int)MB.lastError());
}

static void runScenarios()
{
    for (int i = 0; i < 32; i++) { g_peerRegs[i] = 0x1000 + i; g_peerCoils[i] = i % 3 == 0; }

    // ===== MASTER =====
    MB.setQueuedTxEnabled(false);
    bool ok = MB.begin(247, 115200, SERIAL_8N1);
    printf("begin ok=%d ready=%d baud=%lu\n", ok, MB.isReady(), (unsigned long)MB.effectiveBaudRate());
    g_responder = peer;

    uint16_t regs[8] = {0};
    ok = MB.readHoldingRegistersSync(1, 2, 3, regs, 200);
    report("FC03 sync ok", ok, (int)MB.lastError(), {regs[0], regs[1], regs[2]});

    ok = MB.writeSingleRegisterSync(1, 5, 0xBEEF, 200);
    report("FC06 sync ok", ok, (int)MB.lastError(), {g_peerRegs[5]});

    const uint16_t src[4] = {1, 2, 3, 4};
    ok = MB.writeMultipleRegistersSync(1, 0x0084, 4, src, 200);
    report("FC16 sync ok", ok, (int)MB.lastError());

    ok = MB.readInputRegistersSync(1, 0, 2, regs, 200);
    report("FC04 sync ok", ok, (int)MB.lastError(), {regs[0], regs[1]});

    uint8_t bits[4] = {0};
    ok = MB.readCoilsSync(1, 0, 10, bits, 200);
    report("FC01 sync ok", ok, (int)MB.lastError(), {bits[0], bits[1]});

    ok = MB.readDiscreteInputsSync(1, 3, 5, bits, 200);
    report("FC02 sync ok", ok, (int)MB.lastError(), {bits[0]});

    ok = MB.writeSingleCoilSync(1, 7, true, 200);
    report("FC05 sync ok", ok, (int)MB.lastError(), {g_peerCoils[7]});

    const uint8_t coils[2] = {0xA5, 0x01};
    ok = MB.writeMultipleCoilsSync(1, 0, 9, coils, 200);
    report("FC15 sync ok", ok, (int)MB.lastError());

    g_responder = [](const std::vector<uint8_t> &) { return std::vector<uint8_t>(); };
    ok = MB.readHoldingRegistersSync(1, 0, 1, regs, 150);
    report("FC03 timeout", ok, (int)MB.lastError());

    g_responder = [](const std::vector<uint8_t> &r) { return frame({r[0], (uint8_t)(r[1] | 0x80), 0x02}); };
    ok = MB.readHoldingRegistersSync(1, 0, 1, regs, 200);
    report("FC03 exception", ok, (int)MB.lastError());

    g_responder = [](const std::vector<uint8_t> &r) { auto f = peer(r); f.back() ^= 0x55; return f; };
    ok = MB.readHoldingRegistersSync(1, 0, 1, regs, 200);
    report("FC03 bad CRC", ok, (int)MB.lastError());

    g_responder = [](const std::vector<uint8_t> &r) { auto f = peer(r); f[0] = 9; uint16_t c = MB_CLASS::crc16(f.data(), f.size() - 2); f[f.size() - 2] = c & 0xFF; f[f.size() - 1] = c >> 8; return f; };
    ok = MB.readHoldingRegistersSync(1, 0, 1, regs, 200);
    report("FC03 wrong slave reply", ok, (int)MB.lastError());

    // Async con el motor por defecto del Master.
    g_responder = peer;
    MB.motor(MB_ASYNC);
    MB.setQueuedTxEnabled(true);
    ok = MB.requestReadHoldingRegisters(1, 10, 2, regs, 300);
    int polls = 0;
    while (MB.masterBusy() && polls < 10000) { MB.task(); yield(); polls++; }
    report("FC03 async", ok && MB.masterSucceeded(), (int)MB.masterResult(), {regs[0], regs[1], (uint16_t)polls});
    MB.clearMasterResult();

    ok = MB.readHoldingRegisters(1, 11, 1, regs, 300);  // API unificada en ASYNC
    report("FC03 unified accept", ok, (int)MB.masterState());
    while (MB.masterBusy()) { MB.task(); yield(); }
    MB.clearMasterResult();

    MB.motor(MB_SYNC);
    ok = MB.readHoldingRegisters(1, 12, 1, regs, 300);  // API unificada en SYNC
    report("FC03 unified sync", ok, (int)MB.lastError(), {regs[0]});

    printf("stats tx=%lu rx=%lu rxB=%lu ok=%lu crc=%lu to=%lu\n",
           (unsigned long)MB.stats().txFrames, (unsigned long)MB.stats().rxFrames,
           (unsigned long)MB.stats().rxBytes,
           (unsigned long)MB.stats().requestsOk, (unsigned long)MB.stats().crcErrors,
           (unsigned long)MB.stats().masterTimeouts);

    // ===== SLAVE =====
    MB.end();
    static uint16_t holding[8] = {10, 11, 12, 13, 14, 15, 16, 17};
    static uint16_t input[4] = {20, 21, 22, 23};
    static uint8_t coilMap[2] = {0x0F, 0x00};
    static const uint8_t diMap[1] = {0x5A};
    MB.setHoldingRegisters(holding, 8);
    MB.setInputRegisters(input, 4);
    MB.setCoils(coilMap, 16);
    MB.setDiscreteInputs(diMap, 8);
    MB.resetStats();
    ok = MB.begin(2, 19200, SERIAL_8E1);
    printf("slave begin ok=%d\n", ok);
    g_responder = nullptr;

    runSlaveFrame("S FC03 read 3", frame({2, 3, 0, 1, 0, 3}));
    runSlaveFrame("S FC06 write", frame({2, 6, 0, 4, 0x12, 0x34}));
    printf("  holding[4]=%04X\n", holding[4]);
    runSlaveFrame("S FC16 write 2", frame({2, 16, 0, 6, 0, 2, 4, 0xAA, 0xBB, 0xCC, 0xDD}));
    printf("  holding[6..7]=%04X %04X\n", holding[6], holding[7]);
    runSlaveFrame("S FC04 read", frame({2, 4, 0, 0, 0, 4}));
    runSlaveFrame("S FC01 read", frame({2, 1, 0, 0, 0, 10}));
    runSlaveFrame("S FC02 read", frame({2, 2, 0, 0, 0, 8}));
    runSlaveFrame("S FC05 write", frame({2, 5, 0, 9, 0xFF, 0}));
    runSlaveFrame("S FC15 write", frame({2, 15, 0, 0, 0, 4, 1, 0x05}));
    printf("  coils=%02X %02X\n", coilMap[0], coilMap[1]);
    runSlaveFrame("S other id", frame({3, 3, 0, 0, 0, 1}));
    runSlaveFrame("S broadcast FC06", frame({0, 6, 0, 0, 0x55, 0x66}));
    printf("  holding[0]=%04X\n", holding[0]);
    runSlaveFrame("S illegal address", frame({2, 3, 0, 7, 0, 4}));
    runSlaveFrame("S illegal function", frame({2, 0x2B, 0, 0}));
    auto bad = frame({2, 3, 0, 0, 0, 1}); bad.back() ^= 1;
    runSlaveFrame("S bad CRC", bad);
    printf("slave stats rx=%lu rxB=%lu tx=%lu ok=%lu crc=%lu ex=%lu valid=%d coilw=%d\n",
           (unsigned long)MB.stats().rxFrames, (unsigned long)MB.stats().rxBytes,
           (unsigned long)MB.stats().txFrames,
           (unsigned long)MB.stats().requestsOk, (unsigned long)MB.stats().crcErrors,
           (unsigned long)MB.stats().exceptionsSent, MB.hasValidRequest(), MB.hasCoilWrite());
}
