/*
  H_Comunicaciones · Modbus RTU (RS485) y Modbus TCP (Ethernet)

  QUÉ VA AQUÍ:    el mapa de registros que ve el SCADA / HMI externo / otro PLC y su
                  copia desde y hacia las variables de B_Tags.h.
  QUÉ NO VA AQUÍ: lógica (D_Logica). Una orden recibida sólo cambia una variable.

  Se activan en A_Config.h (USAR_MODBUS_RTU / USAR_MODBUS_TCP). El mapa es el mismo
  para los dos: anótalo en docs/MAPA_IO.md, sección «Mapa Modbus».

  Ethernet lo inicia y mantiene el package: no llamar Ethernet.begin() aquí.
*/

// Mapa Modbus (dirección = índice del arreglo):
//   Holding 0 (40001)  estado de la máquina (EstadoMaquina)       lectura
//   Holding 1 (40002)  alarmas activas (bit = CodigoAlarma)        lectura
//   Holding 2 (40003)  alarmas enclavadas                          lectura
//   Holding 3 (40004)  imagen de entradas I0_0..I0_7               lectura
//   Holding 4 (40005)  imagen de salidas Q0_0..Q0_7                lectura
//   Holding 5 (40006)  ordenes: bit0 reconocer alarmas             escritura (se borra sola)
//   Holding 6..15      reserva
static constexpr uint16_t MB_CANTIDAD = 16;
static constexpr uint16_t MB_ORDENES = 5;
uint16_t registrosModbus[MB_CANTIDAD] = {0};

void comunicacionesInicio()
{
#if USAR_MODBUS_RTU
    JWPLC_ModbusRTU.setHoldingRegisters(registrosModbus, MB_CANTIDAD);
    if (!JWPLC_ModbusRTU.begin(MODBUS_RTU_ID, MODBUS_RTU_BAUDIOS, MODBUS_RTU_FORMATO))
        diagnosticoMensaje("Modbus RTU no inicio", JWPLC_ModbusRTU.lastErrorString());
#endif

#if USAR_MODBUS_TCP
    JWPLC_ModbusTCP.setHoldingRegisters(registrosModbus, MB_CANTIDAD);
    if (!JWPLC_ModbusTCP.beginServer(MODBUS_TCP_UNIT_ID, MODBUS_TCP_PUERTO))
        diagnosticoMensaje("Modbus TCP no inicio", "");
#endif
}

void atenderComunicaciones()
{
#if USAR_MODBUS_RTU || USAR_MODBUS_TCP
    // Órdenes recibidas -> variables.
    if (bitRead(registrosModbus[MB_ORDENES], 0))
    {
        reconocerAlarmas = true;
        bitClear(registrosModbus[MB_ORDENES], 0);
    }

    // Variables -> registros de lectura.
    registrosModbus[0] = (uint16_t)estado;
    registrosModbus[1] = alarmasActivas;
    registrosModbus[2] = alarmasEnclavadas;
    registrosModbus[3] = imagenEntradas;
    registrosModbus[4] = imagenSalidas;
#endif

#if USAR_MODBUS_RTU
    JWPLC_ModbusRTU.task(); // debe llamarse en cada ciclo
#endif
    // Modbus TCP lo atiende el package en segundo plano.
}
