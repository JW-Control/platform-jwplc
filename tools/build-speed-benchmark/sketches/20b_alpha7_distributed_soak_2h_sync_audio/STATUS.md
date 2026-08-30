# Estado — Gate 7NB.3A-C

Estado actual: **REV=5 PENDIENTE DE RECOMPILE-CHECK**.

Evidencia previa:

- REV=4 compiló correctamente en Arduino IDE.
- Esa compilación confirmó que el overlay reutiliza el soak 20 completo y resuelve sus librerías/periféricos.

Cambio REV=5 pendiente de validar:

- tabla de patrones distribuida heredada de Gate 21B;
- bitmaps distintos para M2/S1/S2;
- `START` = soak completo + modo SHOW;
- `CLACK` = mismo soak completo con 0x00/0xFF sincronizado;
- `SHOW` permite volver a la secuencia durante runtime;
- loopback multibit: cuenta cada flanco Q 0->1 y valida bitmap I completo;
- nota calculada a partir del bitmap completo, no sólo del popcount;
- volumen configurable `SOAK_BUZZER_VOLUME` (0..255);
- aplicación temporizada en tarea `jwplcSyncIO` Core 1;
- `JWPLC_ModbusRTU` sin cambios en este gate.

Siguiente evidencia requerida:

1. recompilación Arduino IDE limpia con `GATE_RT_REV=5`;
2. upload a M2/S1/S2;
3. `START` durante 60-120 s;
4. comprobar chase, alternancias, ola y espejos;
5. clicks y notas alineados entre nodos;
6. CRC=0, MB_FAIL=0, mismatch=0 y Q/I coherentes;
7. FRAM/RTC/SD/WiFi/Ethernet continúan operativos;
8. si pasa, extender a 5-10 min antes de volver al Gate Ethernet/Core1.

No usar todavía SHOW como endurance de horas: cambia más relés por unidad de tiempo que el walking original.
