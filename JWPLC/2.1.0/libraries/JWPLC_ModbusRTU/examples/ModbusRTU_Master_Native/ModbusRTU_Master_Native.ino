uint16_t values[4];

void setup()
{
    Serial.begin(115200);
    delay(1200);

    // ID local 247 solo para iniciar el stack en modo master.
    JWPLC_ModbusRTU.begin(247, 19200, SERIAL_8E1);
    JWPLC_ModbusRTU.printStatus(Serial);
}

void loop()
{
    bool ok = JWPLC_ModbusRTU.readHoldingRegisters(1, 0, 4, values, 1000);

    if (ok)
    {
        Serial.print("Read OK | HR0: ");
        Serial.print(values[0]);
        Serial.print(" HR1: ");
        Serial.print(values[1]);
        Serial.print(" HR2: ");
        Serial.print(values[2]);
        Serial.print(" HR3: ");
        Serial.println(values[3]);
    }
    else
    {
        Serial.print("Read failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
    }

    delay(1000);
}