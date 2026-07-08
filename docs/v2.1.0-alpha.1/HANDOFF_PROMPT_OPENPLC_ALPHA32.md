# HANDOFF_PROMPT_OPENPLC_ALPHA32.md

Estoy cerrando la etapa de integración OpenPLC del package Arduino JWPLC Basic.

## Estado actual

- JWPLC Basic `v2.0.0` estable sigue intacto.
- `platform-jwplc` no fue modificado.
- OpenPLC Editor v4 reconoce `JWPLC BASIC [2.0.0]`.
- La integración se realizó del lado OpenPLC Editor / Baremetal Arduino.
- La integración funcional OpenPLC está validada con I/O digital y Modbus TCP.

## Archivos modificados del lado OpenPLC

```txt
editor/arduino/examples/Baremetal/hals.json
editor/arduino/src/hal/jwplcbasic.cpp
editor/arduino/examples/Baremetal/ModbusSlave.cpp
editor/arduino/examples/Baremetal/ModbusSlave.h
```

También se agregó/ajustó la imagen preview del JWPLC Basic para el selector de placas.

## Decisiones técnicas

- `hardwareInit()` en `jwplcbasic.cpp` queda vacío/documentado.
- No se toca `EN_IO` desde OpenPLC.
- No se reinicializa TCA6424A desde OpenPLC.
- `pinMask_DIN/AIN/DOUT/AOUT` usa `uint16_t`.
- `updateInputBuffers()` usa `digitalRead()`.
- `updateOutputBuffers()` usa `digitalWrite()`.
- Modbus TCP usa `JWPLC_Ethernet.h` para inicializar W5500.
- Para JWPLC Basic se usa `EthernetServer`, no `WiFiServer`.
- No se usa `ETH.begin()`.

## Validado

- Compilación desde OpenPLC.
- Subida por USB.
- Debugger OpenPLC.
- Blink `Q0_0`.
- Lectura de entradas.
- Activación de salidas.
- Dependencia entrada -> salida.
- Concordancia con TFT.
- DHCP.
- IP `192.168.0.50` en prueba.
- Puerto TCP 502 abierto.
- ModbusTool como master TCP.
- FC01 Read Coils.
- FC02 Read Discrete Inputs.
- Desconexión/reconexión RJ45 sin cuelgue.
- RTU solo funciona.
- TCP solo funciona.

## Pendiente / limitación conocida

Cuando se habilitan TCP + RTU simultáneamente:

```txt
TCP funciona.
RTU no trabaja correctamente.
```

Esto debe quedar como pendiente o fuera de alcance de la primera integración funcional.

## Qué no asumir

- No asumir OpenPLC integrado dentro del package Arduino.
- No modificar `platform-jwplc` sin razón bloqueante.
- No tocar `platform.txt`, `boards.txt`, particiones, FlashFreq ni bootloader.
- No asumir OTA.
- No asumir que OpenPLC reemplaza `JWPLC_ModbusRTU`.

## Siguiente paso recomendado

1. Guardar patch final de OpenPLC.
2. Crear proyecto ejemplo OpenPLC.
3. Documentar instalación y pruebas.
4. Decidir si RTU+TCP simultáneo se resuelve ahora o queda como pendiente explícito.
5. Preparar PR/release del patch si corresponde.
