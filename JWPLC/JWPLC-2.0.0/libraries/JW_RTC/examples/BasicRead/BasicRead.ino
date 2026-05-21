#include <JW_RTC.h>

JW_RTC rtc;

void printDateTime(const JWRTCDateTime& dt)
{
  Serial.print(dt.year);
  Serial.print('/');
  if (dt.month < 10) Serial.print('0');
  Serial.print(dt.month);
  Serial.print('/');
  if (dt.day < 10) Serial.print('0');
  Serial.print(dt.day);
  Serial.print(' ');

  if (dt.hour < 10) Serial.print('0');
  Serial.print(dt.hour);
  Serial.print(':');
  if (dt.minute < 10) Serial.print('0');
  Serial.print(dt.minute);
  Serial.print(':');
  if (dt.second < 10) Serial.print('0');
  Serial.println(dt.second);
}

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

  Serial.println("RTC found");
}

void loop()
{
  JWRTCDateTime dt;
  float tempC = 0.0f;

  if (rtc.read(dt))
  {
    Serial.print("Valid: ");
    Serial.print(rtc.isTimeValid());
    Serial.print(" | ");
    printDateTime(dt);
  }
  else
  {
    Serial.print("Read failed: ");
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
  }

  if (rtc.readTemperatureC(tempC))
  {
    Serial.print("Temp C: ");
    Serial.println(tempC, 2);
  }

  delay(1000);
}