#include <JWPLC_Display_ST7789.h>

void setup()
{
  Serial.begin(115200);
}

void loop()
{
  digitalWrite(Q0_0, HIGH);
  digitalWrite(Q0_1, LOW);
  digitalWrite(Q0_2, HIGH);
  digitalWrite(Q0_3, LOW);
  digitalWrite(Q0_4, HIGH);
  digitalWrite(Q0_5, LOW);
  digitalWrite(Q0_6, HIGH);
  digitalWrite(Q0_7, LOW);
  delay(300);

  digitalWrite(Q0_0, LOW);
  digitalWrite(Q0_1, HIGH);
  digitalWrite(Q0_2, LOW);
  digitalWrite(Q0_3, HIGH);
  digitalWrite(Q0_4, LOW);
  digitalWrite(Q0_5, HIGH);
  digitalWrite(Q0_6, LOW);
  digitalWrite(Q0_7, HIGH);
  delay(300);
}

void loop1()
{
  delay(200);
}