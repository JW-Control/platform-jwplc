#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_GlobalPeripherals.h>

#include "esp_timer.h"

extern "C"
{
extern volatile uint64_t a13_tft_pre2_init_entry_us;
extern volatile uint64_t a13_tft_pre2_rst_low_us;
extern volatile uint64_t a13_tft_pre2_state_end_us;
extern volatile uint64_t a13_tft_pre2_i2c_end_us;
extern volatile uint64_t a13_tft_pre2_rtc_end_us;
extern volatile uint64_t a13_tft_pre2_fram_end_us;
extern volatile uint64_t a13_tft_pre2_sd_end_us;
extern volatile uint64_t a13_tft_pre2_buttons_end_us;
extern volatile uint64_t a13_tft_pre2_display_begin_start_us;
extern volatile uint64_t a13_tft_pre2_display_begin_end_us;
extern volatile uint64_t a13_tft_pre2_display_refresh_end_us;
extern volatile uint64_t a13_tft_pre2_tca_init_end_us;
extern volatile uint64_t a13_tft_pre2_tca_config_end_us;
extern volatile uint64_t a13_tft_pre2_en_io_high_us;
extern volatile uint64_t a13_tft_pre2_init_end_us;
extern volatile bool a13_tft_pre2_display_begin_ok;
}

static uint64_t g_setup_entry_us = 0;

static void printU64(const char *key, uint64_t value)
{
    Serial.print(key);
    Serial.print("=");
    Serial.println((unsigned long long)value);
}

static void printResult()
{
    Serial.println("A13_TFT_PRE2_RESULT=BEGIN");
    printU64("INIT_ENTRY_US", a13_tft_pre2_init_entry_us);
    printU64("RST_LOW_US", a13_tft_pre2_rst_low_us);
    printU64("STATE_END_US", a13_tft_pre2_state_end_us);
    printU64("I2C_END_US", a13_tft_pre2_i2c_end_us);
    printU64("RTC_END_US", a13_tft_pre2_rtc_end_us);
    printU64("FRAM_END_US", a13_tft_pre2_fram_end_us);
    printU64("SD_END_US", a13_tft_pre2_sd_end_us);
    printU64("BUTTONS_END_US", a13_tft_pre2_buttons_end_us);
    printU64("DISPLAY_BEGIN_START_US", a13_tft_pre2_display_begin_start_us);
    printU64("DISPLAY_BEGIN_END_US", a13_tft_pre2_display_begin_end_us);
    printU64("DISPLAY_REFRESH_END_US", a13_tft_pre2_display_refresh_end_us);
    printU64("TCA_INIT_END_US", a13_tft_pre2_tca_init_end_us);
    printU64("TCA_CONFIG_END_US", a13_tft_pre2_tca_config_end_us);
    printU64("EN_IO_HIGH_US", a13_tft_pre2_en_io_high_us);
    printU64("INIT_END_US", a13_tft_pre2_init_end_us);
    printU64("SETUP_ENTRY_US", g_setup_entry_us);
    printU64("PROBE_PRINT_US", (uint64_t)esp_timer_get_time());
    Serial.print("DISPLAY_BEGIN_OK=");
    Serial.println(a13_tft_pre2_display_begin_ok ? "YES" : "NO");
    Serial.print("DISPLAY_READY=");
    Serial.println(JWPLC_Display.isReady() ? "YES" : "NO");
    Serial.print("IO_READY=");
    Serial.println(JWPLC_IO.ready() ? "YES" : "NO");
    Serial.println("A13_TFT_PRE2_RESULT=END");
}

void setup()
{
    g_setup_entry_us = (uint64_t)esp_timer_get_time();
    Serial.begin(115200);
    delay(100);
    printResult();
}

void loop()
{
    static uint32_t lastPrintMs = 0;
    const uint32_t now = millis();

    if ((uint32_t)(now - lastPrintMs) >= 500u)
    {
        lastPrintMs = now;
        printResult();
    }

    delay(10);
}
