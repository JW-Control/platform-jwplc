#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "soc/rtc.h"
#include "Arduino.h"
#include "peripherals_init.h"
#include "jwplc_peripherals.h"

#if (ARDUINO_USB_CDC_ON_BOOT | ARDUINO_USB_MSC_ON_BOOT | ARDUINO_USB_DFU_ON_BOOT) && !ARDUINO_USB_MODE
#include "USB.h"
#if ARDUINO_USB_MSC_ON_BOOT
#include "FirmwareMSC.h"
#endif
#endif

#include "chip-debug-report.h"

#ifndef ARDUINO_LOOP_STACK_SIZE
#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define ARDUINO_LOOP_STACK_SIZE 8192
#else
#define ARDUINO_LOOP_STACK_SIZE CONFIG_ARDUINO_LOOP_STACK_SIZE
#endif
#endif

TaskHandle_t loopTaskHandle = NULL;
TaskHandle_t loop1TaskHandle = NULL;
TaskHandle_t jwplcSystemTaskHandle = NULL;
SemaphoreHandle_t initSemaphore = NULL;

#if CONFIG_AUTOSTART_ARDUINO
#if CONFIG_FREERTOS_UNICORE
void yieldIfNecessary(void)
{
  static uint64_t lastYield = 0;
  uint64_t now = millis();
  if ((now - lastYield) > 2000)
  {
    lastYield = now;
    vTaskDelay(5); // delay 1 RTOS tick
  }
}
#endif

bool loopTaskWDTEnabled;

__attribute__((weak)) size_t getArduinoLoopTaskStackSize(void)
{
  return ARDUINO_LOOP_STACK_SIZE;
}

__attribute__((weak)) size_t getArduinoLoop1TaskStackSize(void)
{
  return ARDUINO_LOOP_STACK_SIZE;
}

__attribute__((weak)) size_t getArduinoJWPLCSystemTaskStackSize(void)
{
  return ARDUINO_LOOP_STACK_SIZE;
}

__attribute__((weak)) bool shouldPrintChipDebugReport(void)
{
  return false;
}

// this function can be changed by the sketch using the macro SET_TIME_BEFORE_STARTING_SKETCH_MS(time_ms)
__attribute__((weak)) uint64_t getArduinoSetupWaitTime_ms(void)
{
  return 0;
}

__attribute__((weak)) uint32_t getJWPLCIoScanPeriod_ms(void)
{
  return 20;
}

__attribute__((weak)) uint32_t getJWPLCRTCPeriod_ms(void)
{
  return 1000;
}

extern "C" __attribute__((weak)) uint32_t jwplcDisplayDesiredPeriod_ms(void)
{
  return 50; // valor por defecto si no hay librería que lo sobrescriba
}

uint32_t getJWPLCDisplayPeriod_ms(void)
{
  uint32_t ms = jwplcDisplayDesiredPeriod_ms();
  return (ms == 0) ? 1 : ms;
}

__attribute__((weak)) uint32_t getJWPLCSystemTaskSleep_ms(void)
{
  return 5;
}

// loop1 por defecto: segura si el usuario no la redefine
void loop1(void) __attribute__((weak));
void loop1(void)
{
  vTaskDelay(1);
}

void loopTask(void *pvParameters)
{
  delay(getArduinoSetupWaitTime_ms());

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  printBeforeSetupInfo();
#else
  if (shouldPrintChipDebugReport())
  {
    printBeforeSetupInfo();
  }
#endif

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
  Serial0.setPins(gpioNumberToDigitalPin(SOC_RX0), gpioNumberToDigitalPin(SOC_TX0));
#endif

  initPeripherals();
  jwplcSystemScanIO(); // dejar inputs ya disponibles antes de setup()
  setup();

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  printAfterSetupInfo();
#else
  if (shouldPrintChipDebugReport())
  {
    printAfterSetupInfo();
  }
#endif

  if (initSemaphore != NULL)
  {
    xSemaphoreGive(initSemaphore);
  }

  for (;;)
  {
#if CONFIG_FREERTOS_UNICORE
    yieldIfNecessary();
#endif
    if (loopTaskWDTEnabled)
    {
      esp_task_wdt_reset();
    }
    loop();
    if (serialEventRun)
    {
      serialEventRun();
    }
  }
}

void loop1Task(void *pvParameters)
{
  if (initSemaphore != NULL)
  {
    xSemaphoreTake(initSemaphore, portMAX_DELAY);
    xSemaphoreGive(initSemaphore);
  }

  for (;;)
  {
#if CONFIG_FREERTOS_UNICORE
    yieldIfNecessary();
#endif
    loop1();
  }
}

void jwplcSystemTask(void *pvParameters)
{
  if (initSemaphore != NULL)
  {
    xSemaphoreTake(initSemaphore, portMAX_DELAY);
    xSemaphoreGive(initSemaphore);
  }

  uint32_t lastIoScan = 0;
  uint32_t lastRtcTick = 0;
  uint32_t lastDisplayTick = 0;

  for (;;)
  {
    uint32_t now = millis();

    if ((uint32_t)(now - lastIoScan) >= getJWPLCIoScanPeriod_ms())
    {
      lastIoScan = now;
      jwplcSystemScanIO();
    }

    if ((uint32_t)(now - lastRtcTick) >= getJWPLCRTCPeriod_ms())
    {
      lastRtcTick = now;
      jwplcSystemTickRTC();
    }

    if ((uint32_t)(now - lastDisplayTick) >= getJWPLCDisplayPeriod_ms())
    {
      lastDisplayTick = now;
      jwplcSystemDisplayHook();
    }

    vTaskDelay(pdMS_TO_TICKS(getJWPLCSystemTaskSleep_ms()));
  }
}

extern "C" void app_main()
{
#ifdef F_XTAL_MHZ
#if !CONFIG_IDF_TARGET_ESP32S2
  rtc_clk_xtal_freq_update((rtc_xtal_freq_t)F_XTAL_MHZ);
  rtc_clk_cpu_freq_set_xtal();
#endif
#endif

#ifdef F_CPU
  setCpuFrequencyMhz(F_CPU / 1000000);
#endif

#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
  Serial.begin();
#endif
#if ARDUINO_USB_MSC_ON_BOOT && !ARDUINO_USB_MODE
  MSC_Update.begin();
#endif
#if ARDUINO_USB_DFU_ON_BOOT && !ARDUINO_USB_MODE
  USB.enableDFU();
#endif
#if ARDUINO_USB_ON_BOOT && !ARDUINO_USB_MODE
  USB.begin();
#endif

  loopTaskWDTEnabled = false;
  initArduino();

  initSemaphore = xSemaphoreCreateBinary();

  xTaskCreateUniversal(
      loopTask,
      "loopTask",
      getArduinoLoopTaskStackSize(),
      NULL,
      1,
      &loopTaskHandle,
      ARDUINO_RUNNING_CORE);

#if CONFIG_FREERTOS_UNICORE
  BaseType_t loop1Core = ARDUINO_RUNNING_CORE;
#else
  BaseType_t loop1Core = (ARDUINO_RUNNING_CORE == 0) ? 1 : 0;
#endif

  xTaskCreateUniversal(
      loop1Task,
      "loop1Task",
      getArduinoLoop1TaskStackSize(),
      NULL,
      1,
      &loop1TaskHandle,
      loop1Core);

  // La tarea del sistema JWPLC se queda con el mismo core de loopTask
  xTaskCreateUniversal(
      jwplcSystemTask,
      "jwplcSystemTask",
      getArduinoJWPLCSystemTaskStackSize(),
      NULL,
      1,
      &jwplcSystemTaskHandle,
      ARDUINO_RUNNING_CORE);
}

#endif