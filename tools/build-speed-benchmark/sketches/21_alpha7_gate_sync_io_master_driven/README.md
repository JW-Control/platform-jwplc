# Gate 7NB.3A — sincronización I/O master-driven

Este gate valida la transición del walking I/O distribuido desde ciclos autónomos por nodo hacia una secuencia gobernada por el Master.

## Objetivo

- M2 decide el canal y el estado ON/OFF.
- S1 y S2 reciben la orden antes de la conmutación.
- Los tres nodos aplican el cambio con una referencia temporal común.
- El resultado audible esperado es un único "click" conjunto por transición, sin desfase perceptible de cientos de milisegundos.

## Restricciones

- No modifica `JWPLC_ModbusRTU`.
- Usa únicamente la API Modbus ya validada en Alpha7.
- No cambia baudrate, framing RTU, terminación ni bias.
- No se mezcla con el Gate Ethernet/Core1.

## Estado

Pendiente de integrar la lógica de disparo común en el soak 20 después de cerrar la revisión concurrente de la librería Modbus en el otro flujo de trabajo.

La estrategia prevista es armar primero los Slaves y ejecutar después una conmutación con deadline futuro común, evitando tres secuencias autónomas iniciadas en momentos distintos.
