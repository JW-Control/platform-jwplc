// Alpha13 H0-F1: ensayo fisico aislado del package corregido, exclusivamente COM4.
// No modifica perifericos, autoload, GPIO4, relés ni pines de hardware.
#include <JWPLC_Ethernet.h>
#include <esp_system.h>

static uint32_t lastReport = 0;
static uint32_t bootId = 0;
static uint32_t reportCount = 0;
static const uint32_t H0_MAGIC = 0x48304631UL;
RTC_NOINIT_ATTR uint32_t h0Magic;
RTC_NOINIT_ATTR uint32_t h0Stage;
RTC_NOINIT_ATTR uint32_t h0Inverse;

void setup() {
  Serial.begin(115200);
  bootId = esp_random();
  Serial.print("H0F1_BOOT="); Serial.println(bootId, HEX);
  if (h0Magic != H0_MAGIC || h0Inverse != ~h0Stage) {
    h0Magic = H0_MAGIC;
    h0Stage = 0;
    h0Inverse = ~h0Stage;
  }
  Serial.print("H0F1_PREVIOUS_STAGE="); Serial.println(h0Stage);
  if (h0Stage == 1) {
    Serial.println("H0F1_REVIEW_STOP_PREVIOUS_EARLY_LINK_NOT_RETURNED");
    return;
  }
  // Reproduce la consulta antes del primer service() del sketch.
  h0Stage = 1;
  h0Inverse = ~h0Stage;
  Serial.println("H0F1_EARLY_LINK_BEGIN");
  EthernetLinkStatus status = JWPLC_Ethernet.linkStatus();
  h0Stage = 2;
  h0Inverse = ~h0Stage;
  Serial.print("H0F1_EARLY_LINK_RETURN="); Serial.println((int)status);
}
void loop() {
  const uint32_t now = millis();
  if ((uint32_t)(now - lastReport) < 1000U) return;
  lastReport = now;
  ++reportCount;
  Serial.print("H0F1_READY:BOOT="); Serial.print(bootId, HEX);
  Serial.print(" COUNT="); Serial.print(reportCount);
  Serial.print(" STAGE="); Serial.print(h0Stage);
  Serial.print(" LINK=");
  if (h0Stage == 2) {
    Serial.print((int)JWPLC_Ethernet.linkStatus()); // PHY real. No usar -9.
  } else {
    Serial.print("UNTESTED");
  }
  Serial.println();
}
