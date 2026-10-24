// Gate A14 fase 2: prueba física combinada del package 2.1.0-alpha.14.
//
// COM6  Master (JWPLC Basic con Ethernet): este sketch. Pantalla del Designer con tres
//        pestañas (Proceso, Comunicaciones, Ajustes con MENU), reloj del RTC,
//        «°C», ICON, BAR, buzzer, Ethernet DHCP + servidor Modbus TCP y
//        Master Modbus RTU.
// COM3  Slave: ejemplo JWPLC_ModbusRTU/01.ModbusRTU_Slave_Holding (ID 2,
//        115200 8N1), unido por RS-485.
//
// Master RTU: FC03 HR0..HR3 del Slave cada 500 ms; al confirmar «Escribir HR1»
// en el MENU, FC06 HR1 = valorHR1 (el Slave lo imprime por su Serial).
// Modbus TCP (Unit ID 1, puerto 502): HR0 = temperatura x10, HR1 = HR0 del
// Slave, HR2 = rtuOk, HR3 = rtuErr; escribir HR10 = 1 enciende la bomba.
//
// Serial 115200: una línea de estado cada 2 s y cada valor confirmado del MENU.
#include <JWPLC_ModbusTCP.h>
#include "JWPLC_HMI_Generated.h"

static constexpr uint8_t RTU_SLAVE_ID = 2;
static constexpr uint32_t RTU_PERIOD_MS = 500;

static uint16_t tcpHolding[16] = {};
static uint16_t rtuValues[4] = {};
static bool rtuWritePending = false;
static bool rtuLastWasWrite = false;
static uint32_t rtuNextMs = 1000;

static void beep(uint16_t ms = 60)
{
    if (pitido)
        JWPLC_Buzzer.beep(ms);
}

static void serviceRtuMaster(uint32_t now)
{
    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            rtuOk++;
            if (!rtuLastWasWrite)
                esclavoHR0 = rtuValues[0];
            else
                Serial.printf("RTU FC06 HR1=%ld OK\n", (long)valorHR1);
        }
        else
        {
            rtuErr++;
            Serial.printf("RTU %s FALLO: %s\n", rtuLastWasWrite ? "FC06" : "FC03",
                          JWPLC_ModbusRTU.lastErrorString());
        }
        JWPLC_ModbusRTU.clearMasterResult();
        rtuNextMs = now + RTU_PERIOD_MS;
    }

    if (JWPLC_ModbusRTU.masterBusy() || JWPLC_ModbusRTU.masterDone() ||
        (int32_t)(now - rtuNextMs) < 0)
        return;

    bool accepted;
    if (rtuWritePending)
    {
        rtuLastWasWrite = true;
        accepted = JWPLC_ModbusRTU.requestWriteSingleRegister(RTU_SLAVE_ID, 1, (uint16_t)valorHR1, 1000);
        if (accepted)
            rtuWritePending = false;
    }
    else
    {
        rtuLastWasWrite = false;
        accepted = JWPLC_ModbusRTU.requestReadHoldingRegisters(RTU_SLAVE_ID, 0, 4, rtuValues, 1000);
    }
    if (!accepted)
    {
        rtuErr++;
        rtuNextMs = now + RTU_PERIOD_MS;
    }
}

void setup()
{
    Serial.begin(115200);
    jwplcHMISetup(); // páginas, campos, MENU e IDLE del Designer

    JWPLC_Ethernet.useDHCP();
    JWPLC_ModbusTCP.setHoldingRegisters(tcpHolding, 16);
    JWPLC_ModbusTCP.beginServer(1, 502);

    if (!JWPLC_ModbusRTU.begin(247, 115200, SERIAL_8N1))
        Serial.printf("Modbus RTU: %s\n", JWPLC_ModbusRTU.lastErrorString());

    JWPLC_Buzzer.setVolume(32);
    beep(120);
    Serial.println("A14_F2_Combinado listo");
}

void loop()
{
    const uint32_t now = millis();
    JWPLC_ModbusRTU.task();
    serviceRtuMaster(now);

    // Proceso simulado: la temperatura sigue a la consigna si la bomba marcha.
    static uint32_t lastSim = 0;
    if (now - lastSim >= 250)
    {
        lastSim = now;
        temperatura += bomba ? (consigna - temperatura) * 0.05f : (20.0f - temperatura) * 0.01f;
        nivel = (float)((now / 100) % 101);
        estadoMotor = bomba ? 1 : 0;
    }

    // Modbus TCP: publicar y leer el mando de la bomba (HR10).
    ethLista = JWPLC_Ethernet.isReady();
    tcpPeticiones = JWPLC_ModbusTCP.stats().requestsOk;
    tcpHolding[0] = (uint16_t)(temperatura * 10.0f);
    tcpHolding[1] = rtuValues[0];
    tcpHolding[2] = (uint16_t)rtuOk;
    tcpHolding[3] = (uint16_t)rtuErr;
    if (tcpHolding[10] <= 1 && (tcpHolding[10] != 0) != bomba)
    {
        bomba = tcpHolding[10] != 0;
        beep();
    }

    uint8_t nodo;
    if (JWPLC_Display.menuChanged(MENU_AJUSTES, &nodo))
    {
        beep();
        switch (nodo)
        {
        case MENU_AJUSTES_CONSIGNA:
            Serial.printf("MENU consigna=%.1f\n", consigna);
            break;
        case MENU_AJUSTES_BOMBA:
            tcpHolding[10] = bomba ? 1 : 0;
            Serial.printf("MENU bomba=%d\n", bomba);
            break;
        case MENU_AJUSTES_ESCRIBIR_HR1:
            rtuWritePending = true;
            Serial.printf("MENU valorHR1=%ld -> FC06\n", (long)valorHR1);
            break;
        case MENU_AJUSTES_PITIDO:
            Serial.printf("MENU pitido=%d\n", pitido);
            break;
        default:
            break;
        }
    }

    static uint32_t lastLog = 0;
    if (now - lastLog >= 2000)
    {
        lastLog = now;
        Serial.printf("ETH=%d IP=%s TCP=%lu RTU ok=%lu err=%lu HR0(Slave)=%u T=%.1f\n",
                      ethLista, JWPLC_Ethernet.localIP().toString().c_str(),
                      (unsigned long)tcpPeticiones, (unsigned long)rtuOk,
                      (unsigned long)rtuErr, rtuValues[0], temperatura);
    }
}
