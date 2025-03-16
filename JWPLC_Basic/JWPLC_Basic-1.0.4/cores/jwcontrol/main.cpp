#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "Arduino.h"
#include "plc-peripherals/include/plc-peripherals.h"
#include "IndustrialShields.h"
#if (ARDUINO_USB_CDC_ON_BOOT|ARDUINO_USB_MSC_ON_BOOT|ARDUINO_USB_DFU_ON_BOOT) && !ARDUINO_USB_MODE
#include "USB.h"
#if ARDUINO_USB_MSC_ON_BOOT
#include "FirmwareMSC.h"
#endif
#endif

#include "Semaphore.h"
#ifndef ARDUINO_LOOP_STACK_SIZE
#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define ARDUINO_LOOP_STACK_SIZE 8192
#else
#define ARDUINO_LOOP_STACK_SIZE CONFIG_ARDUINO_LOOP_STACK_SIZE
#endif
#endif

TaskHandle_t loopTaskHandle = NULL;
TaskHandle_t loop1TaskHandle = NULL;
SemaphoreBinary initSemaphore(false);

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

void serial1EventRun() __attribute__((weak));
void serial2EventRun() __attribute__((weak));

int retinitExpandedGPIO = -1;
int retIS_initDefaultIOPins = -1;
void loopTask(void *pvParameters)
{
	{
	        retinitExpandedGPIO = initExpandedGPIO(true);

		retIS_initDefaultIOPins = IS_initDefaultIOPins();
	}

	setup();

	initSemaphore.give();

	for(;;) {
#if CONFIG_FREERTOS_UNICORE
		yieldIfNecessary();
#endif
		if(loopTaskWDTEnabled){
			esp_task_wdt_reset();
		}
		loop();
		if (serialEventRun) serialEventRun();
		if (serial1EventRun) serial1EventRun();
		if (serial2EventRun) serial2EventRun();
	}
}

void loop1Task(void *pvParameters)
{
	initSemaphore.take();
	initSemaphore.give();

	for(;;) {
		if(loopTaskWDTEnabled){
			esp_task_wdt_reset();
		}
		loop1();
	}
}

extern "C" void app_main()
{
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
  xTaskCreateUniversal(loopTask, "loopTask", getArduinoLoopTaskStackSize(), NULL, 1, &loopTaskHandle, ARDUINO_RUNNING_CORE);
	if (loop1) {
		xTaskCreateUniversal(loop1Task, "loop1Task", getArduinoLoopTaskStackSize(), NULL, 0, &loop1TaskHandle, ARDUINO_RUNNING_CORE == 1 ? 0 : 1);
	}
}

#endif
