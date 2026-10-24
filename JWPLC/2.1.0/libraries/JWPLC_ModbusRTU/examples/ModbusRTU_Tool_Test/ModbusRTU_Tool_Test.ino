/*
  ModbusRTU_Tool_Test

  Este ejemplo configura el JWPLC como un Esclavo Modbus RTU, 
  ideal para pruebas con "JW Modbus Tool".

  Demuestra cómo leer y escribir bloques enteros de I/O (I0_X y Q0_X)
  de forma óptima usando timers para no saturar el bus I2C.
*/

#include <Arduino.h>

// Creamos un arreglo de 2 registros Modbus (Holding Registers)
// Registro 0 (Dirección 0): Guardará el estado de las Entradas I0_X
// Registro 1 (Dirección 1): Recibirá el estado para las Salidas Q0_X
uint16_t modbusRegisters[2] = {0, 0};

// Variable para controlar la velocidad de actualización física
unsigned long ultimoTiempoIO = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // 1. El core ya inicia el bus I2C y habilita el expansor TCA6424A (EN_IO)
  //    antes de setup(): el sketch no debe llamar a Wire.begin() ni tocar EN_IO.

  // 2. Configurar los pines de I/O industriales en bloque
  for(int i = 0; i < 8; i++) {
    pinMode(I0_X[i], INPUT);
    pinMode(Q0_X[i], OUTPUT);
  }

  Serial.println("Iniciando PLC como Esclavo Modbus...");

  // 3. Iniciar el Modbus RTU (Slave ID 1, 9600 Baudios, 8N1)
  if (!JWPLC_ModbusRTU.begin(1, 9600, SERIAL_8N1)) {
    Serial.println("Error al iniciar Modbus RTU.");
    return;
  }

  // 4. Vincular nuestro arreglo para que el Modbus pueda leerlo y escribirlo
  JWPLC_ModbusRTU.setHoldingRegisters(modbusRegisters, 2);

  Serial.println("Esclavo Modbus ID 1 listo en 9600 baudios.");
}

void loop() {
  // 1. Procesar peticiones del bus RS-485 (NO BLOQUEANTE)
  JWPLC_ModbusRTU.task();
  
  // 2. Actualizar Entradas y Salidas físicas (SOLO cada 50ms)
  // ESTO ES CLAVE: Si lo hacemos en cada vuelta del loop sin esta pausa, 
  // saturamos el bus I2C y la pantalla (u otros periféricos I2C) se congela.
  if (millis() - ultimoTiempoIO >= 50) {
    ultimoTiempoIO = millis();

    // Leer las entradas físicas reales (bloque completo) y guardarlas en Registro 0
    modbusRegisters[0] = digitalReadBlock(I0_X);
      
    // Escribir el valor recibido por Modbus (Registro 1) a las salidas reales
    digitalWriteBlock(Q0_X, modbusRegisters[1]);
  }
}
