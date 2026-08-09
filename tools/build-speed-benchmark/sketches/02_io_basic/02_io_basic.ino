void setup()
{
    pinMode(I0_0, INPUT);
    pinMode(Q0_0, OUTPUT);
}

void loop()
{
    digitalWrite(Q0_0, digitalRead(I0_0));
}
