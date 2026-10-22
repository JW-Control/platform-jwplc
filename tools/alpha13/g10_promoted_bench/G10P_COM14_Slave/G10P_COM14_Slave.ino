/*
 * Alpha13 G10 PH0 — COM14 (Slave RTU ID 2, Ethernet TCP Server).
 * Package productivo promovido, sin G10-R ni instrumentación RTC.
 * Placa COM14 de banco sin microSD: no se accede a SD.
 * Nunca accionar Q ni escribir FRAM.
 */
#include <JWPLC_Ethernet.h>
#include <JWPLC_ModbusTCP.h>
#include <JWPLC_ModbusRTU.h>
#include <JWPLC_RS485.h>
#include <esp_system.h>

static uint16_t rtuHolding[4] = {0x1234, 0x2345, 0x3456, 0x4567};
static uint16_t tcpHolding[4] = {0x5678, 0x6789, 0x789A, 0x89AB};
static uint32_t bootNonce = 0;
static uint32_t heartbeat = 0;
static uint32_t lastBeatMs = 0;
static bool rtuConfigured = false;
static bool tcpConfigured = false;

void setup() {
  Serial.begin(115200);
  bootNonce = esp_random();
  JWPLC_Ethernet.useDHCP();
  JWPLC_ModbusRTU.setHoldingRegisters(rtuHolding, 4);
  rtuConfigured = JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1);
  if (rtuConfigured) JWPLC_ModbusRTU.resetStats();
  JWPLC_ModbusTCP.setHoldingRegisters(tcpHolding, 4);
  tcpConfigured = JWPLC_ModbusTCP.beginServer(1, 502);
  Serial.print("G10P_COM14_BOOT:NONCE=");
  Serial.print(bootNonce, HEX);
  Serial.print(" VER=G10_PROMOTED_PRODUCT_F137_V1 RTU=");
  Serial.print(rtuConfigured ? 1 : 0);
  Serial.print(" TCP_CFG=");
  Serial.println(tcpConfigured ? 1 : 0);
  if (!rtuConfigured) {
    Serial.print("G10P_COM14_RTU_BEGIN_FAIL:");
    Serial.println(JWPLC_ModbusRTU.lastErrorString());
  }
}

void loop() {
  if (rtuConfigured) JWPLC_ModbusRTU.task();
  const uint32_t now = millis();
  if ((uint32_t)(now - lastBeatMs) < 1000UL) return;
  lastBeatMs = now;
  ++heartbeat;
  const JWPLCModbusRTUStats &stats = JWPLC_ModbusRTU.stats();
  Serial.print("G10P_COM14_HEARTBEAT:BOOT=");
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
  Serial.print(" RTU=");
  Serial.print(JWPLC_RS485.isReady() ? 1 : 0);
  Serial.print(" RTU_REQUESTS=");
  Serial.print(stats.requestsOk);
  Serial.print(" RTU_CRC=");
  Serial.println(stats.crcErrors);
}
