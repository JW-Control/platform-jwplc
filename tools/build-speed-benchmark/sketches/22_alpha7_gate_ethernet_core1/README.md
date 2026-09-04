# Gate 7NB.3B/C — Ethernet / Core1

Este gate aísla los picos de latencia observados en el soak distribuido Alpha7.

## Evidencia previa

- `SERVICE_MAX_US ETH` llegó a ~1 s durante endurance.
- En la reproducción dirigida, `ETH_LAT` llegó a ~455 ms mientras `SERVICE_MAX_US ETH` llegó a ~456 ms.
- Los cambios `ETHNEXT` generaron timeouts de escrituras de control hacia S1/S2 y picos >250 ms en sus loops cuando el cable seguía físicamente conectado a M2.
- Modbus RTU periódico y CRC permanecieron estables durante estas pruebas.

## Objetivos

1. Separar el tiempo de `EthernetClient.connect()`, envío y espera de ACK HTTP.
2. Separar NTP de HTTP para no atribuir el bloqueo al subsistema equivocado.
3. Diferenciar `OWNER_PENDING` de `OWNER_ACTIVE` para que un nodo no latchee `ERR_ETH` mientras el operador todavía mueve el cable.
4. Eliminar escrituras bloqueantes del handoff Ethernet durante runtime.
5. Verificar cable conectado, desconexión y reconexión sin bloquear el loop crítico.

## Restricciones

- No modificar `JWPLC_ModbusRTU` durante este gate.
- No cambiar baudrate, framing RTU, terminación ni bias.
- Mantener el Ethernet W5500 y el autoload normal del JWPLC Basic.
- No mezclar este gate con la sincronización del walking I/O.

## Estado

Preparado para implementación incremental en el soak 20 y/o `JWPLC_Ethernet` una vez cerrada la medición de cada etapa.
