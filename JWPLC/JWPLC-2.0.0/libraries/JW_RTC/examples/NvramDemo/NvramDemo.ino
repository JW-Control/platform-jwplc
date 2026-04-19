#include <JW_RTC.h>

JW_RTC rtc;

struct DemoData
{
  uint32_t bootCounter;
  uint16_t marker;
};

void setup()
{
  Serial.begin(115200);
  delay(300);

  if (!rtc.begin())
  {
    Serial.print("RTC begin failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
    return;
  }

  DemoData data{};
  if (!rtc.nvramReadObject(0, data))
  {
    Serial.print("NVRAM read failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
    return;
  }

  if (data.marker != 0x55AA)
  {
    data.marker = 0x55AA;
    data.bootCounter = 0;
  }

  data.bootCounter++;

  if (!rtc.nvramWriteObject(0, data))
  {
    Serial.print("NVRAM write failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
    return;
  }

  Serial.print("Boot counter: ");
  Serial.println(data.bootCounter);
}

void loop()
{
}