// N1 de JW_RS485 en host. Usa el mock de Arduino de JW_ModbusRTU/extras/test.
#include "JW_RS485.h"
#include <assert.h>

uint64_t g_mockUs = 1000000ULL;
std::vector<MockPinEvent> g_pinEvents;
void yield() { mockAdvanceUs(100); }

static int g_pre = 0, g_post = 0, g_activity = 0;
static void onPre(void *) { g_pre++; }
static void onPost(void *) { g_post++; }
static void onActivity(void *) { g_activity++; }

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); return 1; } } while (0)

int main()
{
    const uint8_t frame[3] = {1, 2, 3};

    // Sin begin(): no transmite y reporta NOT_STARTED.
    {
        HardwareSerial uart;
        JW_RS485 port(uart);
        CHECK(port.write(frame, 3) == 0);
        CHECK(port.lastError() == JW_RS485_NOT_STARTED);
        CHECK(!port.queuedWriteSupported());
    }

    // Pin DE/RE: alto durante TX, bajo antes y despues; TX bloqueante (flush).
    {
        HardwareSerial uart;
        JW_RS485 port(uart, 16, 17);
        port.setDirectionPin(4, true);
        port.setPreTransmitCallback(onPre);
        port.setPostTransmitCallback(onPost);
        g_pinEvents.clear();
        CHECK(port.begin(9600, SERIAL_8N1));
        CHECK(uart.begun && uart.baud == 9600 && uart.rxPin == 16 && uart.txPin == 17);
        CHECK(g_pinEvents.size() == 1 && g_pinEvents[0].pin == 4 && g_pinEvents[0].value == LOW);
        CHECK(port.write(frame, 3) == 3);
        CHECK(g_pinEvents.size() == 3 && g_pinEvents[1].value == HIGH && g_pinEvents[2].value == LOW);
        CHECK(uart.flushCount == 1 && g_pre == 1 && g_post == 1);
        CHECK(!port.queuedWriteSupported());
        CHECK(port.writeQueued(frame, 3) == 3 && uart.flushCount == 2);  // cae a TX bloqueante
        CHECK(uart.tx.size() == 6);
    }

    // DE/RE activo en bajo.
    {
        HardwareSerial uart;
        JW_RS485 port(uart);
        port.setDirectionPin(5, false);
        g_pinEvents.clear();
        port.begin();
        port.write(frame, 1);
        CHECK(g_pinEvents.size() == 3 && g_pinEvents[0].value == HIGH && g_pinEvents[1].value == LOW && g_pinEvents[2].value == HIGH);
    }

    // AutoDirection: buffer TX configurado, writeQueued sin flush.
    {
        HardwareSerial uart;
        JW_RS485 port(uart);
        port.setAutoDirection(512);
        port.setActivityCallback(onActivity);
        g_pinEvents.clear();
        CHECK(port.begin(115200, SERIAL_8E1));
        CHECK(uart.txBufferSize == 512 && port.txBufferSize() == 512);
        CHECK(port.queuedWriteSupported() && port.autoDirection());
        CHECK(port.writeQueued(frame, 3) == 3 && uart.flushCount == 0);
        CHECK(g_pinEvents.empty());
        CHECK(port.lastTxActivityMs() == millis() && g_activity == 1);
        CHECK(port.hasRecentActivity(10));

        uart.rx.push_back(0xAA);
        uart.rx.push_back(0xBB);
        uint8_t buffer[4];
        CHECK(port.readAvailable(buffer, 4) == 2 && buffer[0] == 0xAA && buffer[1] == 0xBB);
        CHECK(g_activity == 2);
        CHECK(strcmp(port.configString(), "SERIAL_8E1") == 0);
        CHECK(port.effectiveBaudRate() == 115200);

        port.end();
        CHECK(!port.isReady() && port.lastError() == JW_RS485_NOT_STARTED);
        CHECK(port.effectiveBaudRate() == 0);
    }

    printf("JW_RS485 N1 PASS\n");
    return 0;
}
