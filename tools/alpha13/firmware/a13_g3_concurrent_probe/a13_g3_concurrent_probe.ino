#include <Arduino.h>
#include <JWPLC_GlobalPeripherals.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "driver/gpio.h"

extern "C" {
void jwplc_digitalWrite(uint16_t pin, uint8_t val);
uint8_t JWPLC_readOutputs(void);
void JWPLC_writeOutputs(uint8_t bitmap);
int jwplcI2C_readReg8(uint8_t address, uint8_t reg, uint8_t *data);
}

static const char kToken[] = "__G3_TOKEN__";
static SemaphoreHandle_t aStart, bStart, aDone, bDone;
static bool tasksOk = false;
static const uint16_t PIN_A = 0x2208u; // Q0_0
static const uint16_t PIN_B = 0x2209u; // Q0_1

void taskA(void *) {
  for (;;) {
    if (xSemaphoreTake(aStart, portMAX_DELAY) != pdTRUE) continue;
    jwplc_digitalWrite(PIN_A, HIGH);
    xSemaphoreGive(aDone);
  }
}

void taskB(void *) {
  for (;;) {
    if (xSemaphoreTake(bStart, portMAX_DELAY) != pdTRUE) continue;
    jwplc_digitalWrite(PIN_B, HIGH);
    xSemaphoreGive(bDone);
  }
}

void runTest() {
  const int kTrials = 150;
  int errors=0, completed=0;
  for (int i=0;i<kTrials;i++) {
    JWPLC_writeOutputs(0);
    uint8_t hardware=0xff;
    if (jwplcI2C_readReg8(0x22,0x05,&hardware)!=0 || hardware!=0) {
      ++errors; break;
    }
    xSemaphoreGive(aStart);
    xSemaphoreGive(bStart);
    bool doneA=(xSemaphoreTake(aDone,pdMS_TO_TICKS(750))==pdTRUE);
    bool doneB=(xSemaphoreTake(bDone,pdMS_TO_TICKS(750))==pdTRUE);
    if (!doneA || !doneB) { ++errors; break; }
    hardware=0;
    if (jwplcI2C_readReg8(0x22,0x05,&hardware)!=0 ||
        hardware!=0x03u || (JWPLC_readOutputs()&0x03u)!=0x03u) {
      ++errors; break;
    }
    ++completed;
  }
  // Falla o éxito: desconectar físicamente los relés antes del reset lógico.
  gpio_set_level((gpio_num_t)EN_IO, 0);
  JWPLC_writeOutputs(0);
  uint8_t finalReg=0xff;
  if (jwplcI2C_readReg8(0x22,0x05,&finalReg)!=0 || finalReg!=0) ++errors;
  Serial.print("G3_RESULT=");
  Serial.println(errors==0 && completed==kTrials ? "PASS" : "FAIL");
  Serial.print("G3_ERRORS="); Serial.println(errors);
  Serial.print("G3_TRIALS="); Serial.println(completed);
  Serial.print("G3_FINAL_OUTPUT="); Serial.println(finalReg);
  Serial.print("G3_DONE="); Serial.println(kToken);
  Serial.flush();
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(350);
  aStart=xSemaphoreCreateBinary();
  bStart=xSemaphoreCreateBinary();
  aDone=xSemaphoreCreateBinary();
  bDone=xSemaphoreCreateBinary();
  tasksOk= (aStart && bStart && aDone && bDone);
  if (tasksOk) {
    BaseType_t a=xTaskCreatePinnedToCore(taskA,"G3_A",3072,nullptr,2,nullptr,0);
    BaseType_t b=xTaskCreatePinnedToCore(taskB,"G3_B",3072,nullptr,2,nullptr,1);
    tasksOk=(a==pdPASS && b==pdPASS);
  }
  if (!JWPLC_IO.ready() || !tasksOk) {
    Serial.println("G3_SETUP=FAIL");
    return;
  }
  JWPLC_writeOutputs(0);
  Serial.print("G3_READY="); Serial.println(kToken);
}

void loop() {
  if (!tasksOk || !JWPLC_IO.ready()) { delay(100); return; }
  if (Serial.available()) {
    String cmd=Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd==String("G3_RUN:")+kToken) {
      runTest();
      tasksOk=false; // Nunca repetir automáticamente una prueba de relés.
    }
  }
  static uint32_t last=0;
  if (millis()-last>=350u && tasksOk) {
    last=millis();
    Serial.print("G3_READY="); Serial.println(kToken);
  }
  delay(5);
}
