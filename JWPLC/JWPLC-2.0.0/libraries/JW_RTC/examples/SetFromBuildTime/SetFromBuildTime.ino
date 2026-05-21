#include <JW_RTC.h>

JW_RTC rtc;

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

  JWRTCDateTime dt;
  if (!JW_RTC::fromBuildTime(__DATE__, __TIME__, dt))
  {
    Serial.println("Build time parse failed");
    return;
  }

  if (!rtc.write(dt))
  {
    Serial.print("RTC write failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
    return;
  }

  Serial.println("RTC updated from build time");
}

void loop()
{
}