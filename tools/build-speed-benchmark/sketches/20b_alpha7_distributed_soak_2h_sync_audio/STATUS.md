# Estado — Gate 7NB.3A-C

Estado actual: **PENDIENTE DE COMPILE-CHECK**.

Implementado:

- base funcional del soak 20 reutilizada sin quitar periféricos;
- I/O Master-driven direccionado, sin broadcast;
- aplicación temporizada en tarea de alta prioridad Core 1;
- loopback Q->I y contadores conservados;
- audio sincronizado no bloqueante;
- volumen configurable `SOAK_BUZZER_VOLUME` (0..255);
- `JWPLC_ModbusRTU` sin cambios en este gate.

Siguiente evidencia requerida:

1. compilación Arduino IDE limpia;
2. upload a M2/S1/S2;
3. `START` 60-120 s;
4. clicks/notas sincronizados;
5. CRC=0, MB_FAIL=0, mismatch=0;
6. si pasa, 5-10 min con Ethernet conectado antes de Gate Ethernet/Core1.
