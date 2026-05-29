#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "soc/rtc.h"
#include "Arduino.h"
#include "peripherals_init.h"

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
SemaphoreHandle_t initSemaphore = NULL;  // FreeRTOS semaphore

void loop1(void) __attribute__((weak));

#if CONFIG_AUTOSTART_ARDUINO
#if CONFIG_FREERTOS_UNICORE
void yieldIfNecessary(void){
	static uint64_t lastYield = 0;
	uint64_t now = millis();
	if((now - lastYield) > 2000) {
		lastYield = now;
		vTaskDelay(5); //delay 1 RTOS tick
	}
}
#endif

bool loopTaskWDTEnabled;

__attribute__((weak)) size_t getArduinoLoopTaskStackSize(void) {
    return ARDUINO_LOOP_STACK_SIZE;
}

__attribute__((weak)) bool shouldPrintChipDebugReport(void)
{
  return false;
}

void loopTask(void *pvParameters)
{
#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SERIAL)
  // sets UART0 (default console) RX/TX pins as already configured in boot or as defined in variants/pins_arduino.h
  Serial0.setPins(gpioNumberToDigitalPin(SOC_RX0), gpioNumberToDigitalPin(SOC_TX0));
#endif
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  printBeforeSetupInfo();
#else
  if (shouldPrintChipDebugReport())
  {
    printBeforeSetupInfo();
  }
#endif

  Serial.println("🟢 initPeripherals()");
  initPeripherals();

  Serial.println("🟢 setup()");
  setup();
  
  // Libera el semáforo para loop1
  Serial.println("🟢 xSemaphoreGive()");
  xSemaphoreGive(initSemaphore);

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  printAfterSetupInfo();
#else
  if (shouldPrintChipDebugReport()){
    printAfterSetupInfo();
  }  
  
#endif

  Serial.println("🟢 entrando al bucle loop()");
  for (;;) {
    Serial.println("🔄 loop() running...");
#if CONFIG_FREERTOS_UNICORE
    yieldIfNecessary();
#endif
    if (loopTaskWDTEnabled) {
      esp_task_wdt_reset();
    }
    loop();
    if (serialEventRun) serialEventRun();
  }
}

// loop1() opcional (tarea secundaria)
void loop1Task(void *pvParameters)
{
  // Espera hasta que loopTask termine setup()
  Serial.println("🟡 esperando semáforo para loop1");
  xSemaphoreTake(initSemaphore, portMAX_DELAY);

  Serial.println("🟡 semáforo recibido");
  xSemaphoreGive(initSemaphore);

  Serial.println("🟡 entrando al bucle loop1()");
  for (;;)
  {
    Serial.println("🔄 loop1() running...");
    if (loopTaskWDTEnabled)
    {
      esp_task_wdt_reset();
    }
    loop1();
  }
}

extern "C" void app_main()
{
#ifdef F_XTAL_MHZ
#if !CONFIG_IDF_TARGET_ESP32S2 // ESP32-S2 does not support rtc_clk_xtal_freq_update
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
  Serial.begin(115200);

  // Crea semáforo binario
  initSemaphore = xSemaphoreCreateBinary();

  // Lanza loopTask en el core principal
  xTaskCreateUniversal(loopTask, "loopTask", getArduinoLoopTaskStackSize(), NULL, 1, &loopTaskHandle, ARDUINO_RUNNING_CORE);

  // Si loop1() está definido, lanza loop1Task en el otro core
  if (loop1)
  {
    xTaskCreateUniversal(loop1Task, "loop1Task", getArduinoLoopTaskStackSize(), NULL, 0, &loop1TaskHandle, ARDUINO_RUNNING_CORE == 1 ? 0 : 1);
  }
}
#endif