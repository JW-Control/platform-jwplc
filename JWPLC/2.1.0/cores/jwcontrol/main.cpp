#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"
#include "soc/rtc.h"
#include "Arduino.h"
#include "peripherals_init.h"
#include "jwplc_peripherals.h"
#include "jwplc_h3e0b_profile.h"
#include <string.h>

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

// ============================================================================
// Alpha14 H3E.0B - profiler ligero del runtime fuera del loop()
// ============================================================================

static bool g_h3e0b_enabled = false;
static JWPLCH3E0BStats g_h3e0b_stats = {};

static uint32_t g_h3e0b_outside_start_us = 0;
static uint32_t g_h3e0b_pending_tcp_post_us = 0;
static uint32_t g_h3e0b_pending_serial_event_us = 0;
static uint32_t g_h3e0b_pending_task_yield_us = 0;

static uint32_t g_h3e0b_sys_active_start_total = 0;
static uint32_t g_h3e0b_sys_io_start_total = 0;
static uint32_t g_h3e0b_sys_rtc_start_total = 0;
static uint32_t g_h3e0b_sys_eth_start_total = 0;
static uint32_t g_h3e0b_sys_datalog_start_total = 0;
static uint32_t g_h3e0b_sys_display_start_total = 0;

extern "C" __attribute__((weak)) bool jwplcH3E0BProfilerEnabled(void)
{
  return false;
}

static inline void h3e0bAdd(
    JWPLCH3E0BStageStats &stats,
    uint32_t elapsed_us)
{
  ++stats.calls;
  stats.total_us += elapsed_us;
  if (elapsed_us > stats.max_us)
  {
    stats.max_us = elapsed_us;
  }
}

static inline void h3e0bSnapshotSystemStarts(void)
{
  g_h3e0b_sys_active_start_total = g_h3e0b_stats.system_active.total_us;
  g_h3e0b_sys_io_start_total = g_h3e0b_stats.system_io.total_us;
  g_h3e0b_sys_rtc_start_total = g_h3e0b_stats.system_rtc.total_us;
  g_h3e0b_sys_eth_start_total = g_h3e0b_stats.system_ethernet.total_us;
  g_h3e0b_sys_datalog_start_total = g_h3e0b_stats.system_datalog.total_us;
  g_h3e0b_sys_display_start_total = g_h3e0b_stats.system_display.total_us;
}

extern "C" void jwplcH3E0BReset(void)
{
  vTaskSuspendAll();

  memset(&g_h3e0b_stats, 0, sizeof(g_h3e0b_stats));

  g_h3e0b_outside_start_us = 0;
  g_h3e0b_pending_tcp_post_us = 0;
  g_h3e0b_pending_serial_event_us = 0;
  g_h3e0b_pending_task_yield_us = 0;

  h3e0bSnapshotSystemStarts();

  (void)xTaskResumeAll();
}

extern "C" void jwplcH3E0BSnapshot(JWPLCH3E0BStats *out)
{
  if (out == nullptr)
  {
    return;
  }

  vTaskSuspendAll();
  *out = g_h3e0b_stats;
  (void)xTaskResumeAll();
}

static void h3e0bFinishOutside(
    uint32_t tcp_pre_us,
    uint32_t outside_end_us)
{
  if (!g_h3e0b_enabled ||
      g_h3e0b_outside_start_us == 0)
  {
    return;
  }

  const uint32_t outside_us =
      (uint32_t)(
          outside_end_us -
          g_h3e0b_outside_start_us);

  h3e0bAdd(
      g_h3e0b_stats.outside_total,
      outside_us);

  const uint32_t direct_sum_us =
      g_h3e0b_pending_tcp_post_us +
      g_h3e0b_pending_serial_event_us +
      g_h3e0b_pending_task_yield_us +
      tcp_pre_us;

  const uint32_t residual_us =
      outside_us > direct_sum_us
          ? outside_us - direct_sum_us
          : 0U;

  if (outside_us > g_h3e0b_stats.worst_outside_us)
  {
    // Congelar el scheduler sólo durante la captura del nuevo récord evita
    // mezclar trabajo del siguiente turno de jwplcSystemTask en este gap.
    vTaskSuspendAll();

    g_h3e0b_stats.worst_outside_us = outside_us;
    g_h3e0b_stats.worst_tcp_pre_us = tcp_pre_us;
    g_h3e0b_stats.worst_tcp_post_us =
        g_h3e0b_pending_tcp_post_us;
    g_h3e0b_stats.worst_serial_event_us =
        g_h3e0b_pending_serial_event_us;
    g_h3e0b_stats.worst_task_yield_us =
        g_h3e0b_pending_task_yield_us;
    g_h3e0b_stats.worst_direct_sum_us =
        direct_sum_us;
    g_h3e0b_stats.worst_residual_us =
        residual_us;

    g_h3e0b_stats.worst_system_active_delta_us =
        g_h3e0b_stats.system_active.total_us -
        g_h3e0b_sys_active_start_total;
    g_h3e0b_stats.worst_system_io_delta_us =
        g_h3e0b_stats.system_io.total_us -
        g_h3e0b_sys_io_start_total;
    g_h3e0b_stats.worst_system_rtc_delta_us =
        g_h3e0b_stats.system_rtc.total_us -
        g_h3e0b_sys_rtc_start_total;
    g_h3e0b_stats.worst_system_ethernet_delta_us =
        g_h3e0b_stats.system_ethernet.total_us -
        g_h3e0b_sys_eth_start_total;
    g_h3e0b_stats.worst_system_datalog_delta_us =
        g_h3e0b_stats.system_datalog.total_us -
        g_h3e0b_sys_datalog_start_total;
    g_h3e0b_stats.worst_system_display_delta_us =
        g_h3e0b_stats.system_display.total_us -
        g_h3e0b_sys_display_start_total;

    (void)xTaskResumeAll();
  }
}

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

// El runtime integrado (I/O, RTC, Ethernet y Display) debe conservar una
// ventana de servicio aunque el sketch tenga un loop() intensivo que nunca
// invoque delay()/yield(). Se deja como hook débil para diagnóstico avanzado,
// pero la prioridad normal del sistema JWPLC queda por encima del loop de
// usuario (prioridad 1).
__attribute__((weak)) UBaseType_t getJWPLCSystemTaskPriority(void)
{
  return 2;
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

__attribute__((weak)) uint32_t getJWPLCEthernetPeriod_ms(void)
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

extern "C" void jwplcModbusTCPLoopServiceCallback(void) __attribute__((weak));
extern "C" void jwplcModbusTCPLoopServiceCallback(void)
{
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

  // El hook fuerte del sketch de qualification activa H3E.0B.
  // En cualquier build normal permanece false y no se cronometra el runtime.
  g_h3e0b_enabled =
      jwplcH3E0BProfilerEnabled();

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

    // H3E.0B: medir por separado el servicio TCP previo al loop.
    uint32_t tcpPreUs = 0;
    if (g_h3e0b_enabled)
    {
      const uint32_t t0 = micros();
      jwplcModbusTCPLoopServiceCallback();
      tcpPreUs = (uint32_t)(micros() - t0);
      h3e0bAdd(g_h3e0b_stats.tcp_pre, tcpPreUs);
      h3e0bFinishOutside(tcpPreUs, micros());
    }
    else
    {
      jwplcModbusTCPLoopServiceCallback();
    }

    loop();

    if (g_h3e0b_enabled)
    {
      // Desde aquí empieza exactamente el tramo invisible al sketch.
      g_h3e0b_outside_start_us = micros();
      h3e0bSnapshotSystemStarts();

      uint32_t t0 = micros();
      jwplcModbusTCPLoopServiceCallback();
      g_h3e0b_pending_tcp_post_us =
          (uint32_t)(micros() - t0);
      h3e0bAdd(
          g_h3e0b_stats.tcp_post,
          g_h3e0b_pending_tcp_post_us);

      t0 = micros();
      if (serialEventRun)
      {
        serialEventRun();
      }
      g_h3e0b_pending_serial_event_us =
          (uint32_t)(micros() - t0);
      h3e0bAdd(
          g_h3e0b_stats.serial_event,
          g_h3e0b_pending_serial_event_us);

      // El tiempo alrededor de taskYIELD incluye cuánto tarda loopTask
      // en recuperar CPU después de cederla.
      t0 = micros();
      taskYIELD();
      g_h3e0b_pending_task_yield_us =
          (uint32_t)(micros() - t0);
      h3e0bAdd(
          g_h3e0b_stats.task_yield,
          g_h3e0b_pending_task_yield_us);
    }
    else
    {
      jwplcModbusTCPLoopServiceCallback();

      if (serialEventRun)
      {
        serialEventRun();
      }

      taskYIELD();
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
  uint32_t lastEthernetTick = 0;
  uint32_t lastDisplayTick = 0;

  for (;;)
  {
    const uint32_t activeStartUs =
        g_h3e0b_enabled
            ? micros()
            : 0U;

    uint32_t now = millis();

    if ((uint32_t)(now - lastIoScan) >= getJWPLCIoScanPeriod_ms())
    {
      lastIoScan = now;

      if (g_h3e0b_enabled)
      {
        const uint32_t t0 = micros();
        jwplcSystemScanIO();
        h3e0bAdd(
            g_h3e0b_stats.system_io,
            (uint32_t)(micros() - t0));
      }
      else
      {
        jwplcSystemScanIO();
      }
    }

    if ((uint32_t)(now - lastRtcTick) >= getJWPLCRTCPeriod_ms())
    {
      lastRtcTick = now;

      if (g_h3e0b_enabled)
      {
        const uint32_t t0 = micros();
        jwplcSystemTickRTC();
        h3e0bAdd(
            g_h3e0b_stats.system_rtc,
            (uint32_t)(micros() - t0));
      }
      else
      {
        jwplcSystemTickRTC();
      }
    }

    if ((uint32_t)(now - lastEthernetTick) >= getJWPLCEthernetPeriod_ms())
    {
      lastEthernetTick = now;

      if (g_h3e0b_enabled)
      {
        const uint32_t t0 = micros();
        jwplcEthernetTickCallback();
        h3e0bAdd(
            g_h3e0b_stats.system_ethernet,
            (uint32_t)(micros() - t0));
      }
      else
      {
        jwplcEthernetTickCallback();
      }
    }

#if JWPLC_HAS_SD
    // DataLog usa buffer RAM. Este tick solo comprueba threshold/timeout
    // y atiende como maximo un logger por vuelta del system task.
    if (g_h3e0b_enabled)
    {
      const uint32_t t0 = micros();
      jwplcDataLogTickCallback();
      h3e0bAdd(
          g_h3e0b_stats.system_datalog,
          (uint32_t)(micros() - t0));
    }
    else
    {
      jwplcDataLogTickCallback();
    }
#endif

    if ((uint32_t)(now - lastDisplayTick) >= getJWPLCDisplayPeriod_ms())
    {
      lastDisplayTick = now;

      if (g_h3e0b_enabled)
      {
        const uint32_t t0 = micros();
        jwplcSystemDisplayHook();
        h3e0bAdd(
            g_h3e0b_stats.system_display,
            (uint32_t)(micros() - t0));
      }
      else
      {
        jwplcSystemDisplayHook();
      }
    }

    if (g_h3e0b_enabled)
    {
      h3e0bAdd(
          g_h3e0b_stats.system_active,
          (uint32_t)(
              micros() -
              activeStartUs));
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

  // La tarea del sistema JWPLC se queda con el mismo core de loopTask y una
  // prioridad ligeramente superior para garantizar servicio determinista del
  // runtime integrado incluso ante un loop() sin esperas cooperativas.
  xTaskCreateUniversal(
      jwplcSystemTask,
      "jwplcSystemTask",
      getArduinoJWPLCSystemTaskStackSize(),
      NULL,
      getJWPLCSystemTaskPriority(),
      &jwplcSystemTaskHandle,
      ARDUINO_RUNNING_CORE);
}

#endif