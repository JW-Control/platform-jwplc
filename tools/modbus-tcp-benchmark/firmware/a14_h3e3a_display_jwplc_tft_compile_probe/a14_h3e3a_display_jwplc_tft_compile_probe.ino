/*
  A14 H3E.3A - JWPLC_Display source migration compile probe
*/
#include <Arduino.h>
#include <JWPLC_Display.h>

#ifdef ADAFRUIT_ST7789_H
#error "H3E3A: Adafruit_ST7789 leaked to sketch"
#endif

static const JWPLC_UIField FIELDS[] = {
    JWPLC_UIValueField(
        1, 8, 42, "TCP", nullptr,
        JWPLC_UIValueFormat(4, 0, false, false), 0),
    JWPLC_UIBoolField(
        2, 8, 76, "RUN",
        JWPLC_UIBoolText("OFF", "ON"), 0)
};

void setup()
{
    JWPLC_Display.setFields(
        FIELDS,
        sizeof(FIELDS) / sizeof(FIELDS[0]));

    JWPLC_Display.setUserPageCount(1);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setUserRefreshPeriodMs(100);
    JWPLC_Display.setValue(1, 1234);
    JWPLC_Display.setBool(2, true);

    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    JWPLC_TFTClass &alias = JWPLC_Display.display();

    (void)tft.width();
    (void)tft.height();
    (void)alias.isReady();
}

void loop() {}
