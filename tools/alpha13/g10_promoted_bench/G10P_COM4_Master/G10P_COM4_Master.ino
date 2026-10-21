/*
 * Alpha13 G10 PH0 - COM4 MASTER, producto REAL, no overlay G10-R.
 * Sin GPIO4, sin escrituras Q/FRAM/SD, sin escrituras a flash.
 * Puerto USB Serial 115200. RS-485 requiere conexión A/B de banco.
 * Ethernet por autoload normal; nunca Ethernet.begin() manual.
 */
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_Ethernet.h>
#include <JWPLC_ModbusTCP.h>
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>
#include <JWPLC_Display.h>
#include <JWPLC_W5500_SafeAccess.h>
#include <esp_system.h>
#include <string.h>

static uint16_t holding[4] = {0x1234, 0x2345, 0x3456, 0x4567};
static uint16_t rtuValues[4] = {};
static EthernetUDP udp;
static bool udpStarted = false;
static bool udpPending = false;
static bool rtuStarted = false;
static bool tcpConfigured = false;
static uint32_t bootNonce = 0;
static uint32_t heartbeat = 0;
static uint32_t lastPrint = 0, lastRtu = 0, lastSample = 0;
static uint32_t rtuOk = 0, rtuFail = 0, udpRx = 0, udpTx = 0;
static uint32_t sdReadOk = 0, sdReadFail = 0, framRead = 0;
static uint8_t packet[96];

static void samplePeripherals() {
  if (JWPLC_SD.isCardPresent() && JWPLC_SD.isReady()) {
    JWPLCFile root = JWPLC_SD.open("/", FILE_READ);
    if (root) {
      if (root.isDirectory()) ++sdReadOk; else ++sdReadFail;
      root.close();
    } else ++sdReadFail;
  } else ++sdReadFail;
  if (JWPLC_FRAM.size() > 0) {
    volatile uint8_t value = JWPLC_FRAM.read8(0);
    (void)value;
    ++framRead;
  }
}
static void serviceRtu(uint32_t now) {
  if (!rtuStarted) return;
  JWPLC_ModbusRTU.task();
  if (JWPLC_ModbusRTU.masterDone()) {
    bool ok = JWPLC_ModbusRTU.masterSucceeded() &&
      rtuValues[0] == 0x1234 && rtuValues[1] == 0x2345 &&
      rtuValues[2] == 0x3456 && rtuValues[3] == 0x4567;
    if (ok) ++rtuOk; else ++rtuFail;
    JWPLC_ModbusRTU.clearMasterResult();
    lastRtu = now;
  }
  if (!JWPLC_ModbusRTU.masterBusy() && !JWPLC_ModbusRTU.masterDone() &&
      (uint32_t)(now - lastRtu) >= 800UL) {
    if (!JWPLC_ModbusRTU.requestReadHoldingRegisters(2, 0, 4, rtuValues, 1200))
      ++rtuFail;
    lastRtu = now;
  }
}
static void serviceUdp() {
  if (!JWPLC_Ethernet.isReady()) return;
  jwplc_w5500_candidate::BusScope bus(20);
  if (!bus) return;
  if (!udpStarted) {
    udpStarted = udp.begin(15020) == 1;
    return;
  }
  if (udpPending) {
    int result = udp.pollEndPacketAsync();
    if (result != 0) {
      udpPending = false;
      if (result == 1) ++udpTx;
    }
    return;
  }
  int available = udp.parsePacket();
  if (available <= 0) return;
  if (available >= (int)sizeof(packet)) {
    while (udp.available() > 0) (void)udp.read();
    return;
  }
  int n = udp.read(packet, (size_t)available);
  if (n != available || n < 1) return;
  ++udpRx;
  IPAddress ip = udp.remoteIP();
  uint16_t port = udp.remotePort();
  if (udp.beginPacket(ip, port) != 1) return;
  if (udp.write(packet, (size_t)n) != (size_t)n) return;
  int started = udp.beginEndPacketAsync();
  if (started == 0) udpPending = true;
  else if (started == 1) ++udpTx;
}
void setup() {
  Serial.begin(115200);
  bootNonce = esp_random();
  JWPLC_Ethernet.useDHCP();
  JWPLC_ModbusTCP.setHoldingRegisters(holding, 4);
  tcpConfigured = JWPLC_ModbusTCP.beginServer(1, 502);
  rtuStarted = JWPLC_ModbusRTU.begin(247, 115200, SERIAL_8N1);
  Serial.print("G10P_COM4_BOOT:NONCE=");
  Serial.print(bootNonce, HEX);
  Serial.print(" VER=G10_PROMOTED_PRODUCT_F137_V1 TCP_CFG=");
  Serial.print(tcpConfigured ? 1 : 0);
  Serial.print(" RTU=");
  Serial.println(rtuStarted ? 1 : 0);
}
void loop() {
  const uint32_t now = millis();
  serviceRtu(now);
  serviceUdp();
  if ((uint32_t)(now - lastSample) >= 750UL) {
    lastSample = now;
    samplePeripherals();
  }
  if ((uint32_t)(now - lastPrint) < 1000UL) return;
  lastPrint = now;
  ++heartbeat;
  Serial.print("G10P_COM4_HEARTBEAT:BOOT=");
  Serial.print(bootNonce, HEX);
  Serial.print(" HB=");
  Serial.print(heartbeat);
  Serial.print(" PHY=");
  Serial.print((int)JWPLC_Ethernet.linkStatus());
  Serial.print(" IP=");
  Serial.print(JWPLC_Ethernet.isReady() ? JWPLC_Ethernet.localIP() : IPAddress(0, 0, 0, 0));
  Serial.print(" ETH=");
  Serial.print(JWPLC_Ethernet.isReady() ? 1 : 0);
  Serial.print(" TCP=");
  Serial.print(JWPLC_ModbusTCP.serverReady() ? 1 : 0);
  Serial.print(" UDP_RX=");
  Serial.print(udpRx);
  Serial.print(" UDP_TX=");
  Serial.print(udpTx);
  Serial.print(" RTU_OK=");
  Serial.print(rtuOk);
  Serial.print(" RTU_FAIL=");
  Serial.print(rtuFail);
  Serial.print(" SD_OK=");
  Serial.print(sdReadOk);
  Serial.print(" SD_FAIL=");
  Serial.print(sdReadFail);
  Serial.print(" FRAM_READ=");
  Serial.print(framRead);
  Serial.print(" RTC=");
  Serial.print(JWPLC_Time.valid() ? 1 : 0);
  Serial.print(" IO=");
  Serial.print(JWPLC_IO.ready() ? 1 : 0);
  Serial.print(" TFT=");
  Serial.println(JWPLCDisplay::isReady() ? 1 : 0);
}
