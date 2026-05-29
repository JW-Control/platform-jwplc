uint16_t holdingRegs[8];

void setup()
{
    Serial.begin(115200);
    delay(1200);

    for (uint8_t i = 0; i < 8; i++)
    {
        holdingRegs[i] = 1000 + i;
    }

    JWPLC_ModbusRTU.setHoldingRegisters(holdingRegs, 8);
    JWPLC_ModbusRTU.begin();
    JWPLC_ModbusRTU.printStatus(Serial);
}

void loop()
{
    holdingRegs[0] = millis() / 1000;
    JWPLC_ModbusRTU.task();
}