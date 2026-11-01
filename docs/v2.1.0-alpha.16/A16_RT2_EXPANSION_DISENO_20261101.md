# A16 RT2 — Módulos de expansión del Runtime por Modbus RTU (diseño)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/runtime-expansion`, desde `…/feature/runtime-mvp` (PR #203).
Estado: **aprobado por el propietario el 01/11** con las respuestas de §8; en implementación.

```text
A16_RT2_DISENO=PASS              aprobado 01/11 (§8)
A16_RT2_ESCLAVO=PASS_SIM         [ejecutado] JWPLC_Expansion 0.1.0 + Expansion_Esclavo_Basic, ver §10
A16_RT2_MAESTRO=PENDING          Configuración > Expansión en el Runtime
A16_RT2_SIM=PENDING              maestro y esclavo en JWPLC Sim (dos equipos en el mismo RS-485)
A16_RT2_FISICO=PENDING           dos JWPLC Basic por RS-485
```

## 1. Idea (propietario, 01/11)

- Los módulos se conectan **de a uno**. Cada uno trae precargado el sketch de esclavo de su modelo.
- Desde el Runtime del maestro se reconoce el módulo y se le asigna ID, baudrate, trama y **estado seguro**. El
  esclavo guarda todo en su FRAM y queda vinculado.
- Un «barrido de reconocimiento» muestra qué esclavos hay en el bus y cómo están configurados.
- Si el esclavo se configuró desde su sketch, queda **bloqueado**: el maestro lee sus parámetros y los muestra, pero no
  los sobreescribe.
- Primer caso: un JWPLC Basic como esclavo.

## 2. Qué ya existe (no se reinventa)

| Documento / código | Qué aporta |
|---|---|
| `docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_PROTOCOL.md` | Protocolo Remote I/O v1: mapa I/O (FC01/02/05/15 en 0..7), identificación FC04 240..255 (firma «JWPL», producto, firmware, MAC, UID), configuración FC03/06/16 224..239 con clave 0xA55A y comandos. Todo dentro de 0..255 por compatibilidad con OpenPLC. |
| `docs/v2.1.0-alpha.12/BACKPLANE_RTU_CONFIGURATION.md` | Backplane del VPP: hasta 7 módulos, bus del maestro (baud/trama comunes), ciclo FC15 → FC01 → FC02, 3 fallos = fuera de línea con entradas a 0, sondeo de 1 s a los caídos. Failsafe del esclavo ≥ 1000 ms. |
| `JWPLC_ModbusRTU` | Maestro cooperativo no bloqueante (`request…` + `task()`), esclavo con mapas y `lastCoilWriteMs()` para el failsafe. |
| `examples/JWPLC_RemoteIO_Slave_RTU` | Esclavo validado en placa (ID fijo, failsafe 1000 ms, salidas a 0). |

El commissioning por UID con ventanas anti-colisión (PoC 3 de alpha.3) queda fuera: conectar de a uno lo hace
innecesario. Un esclavo hecho para el Runtime sigue funcionando como módulo del Backplane de OpenPLC (mismo mapa I/O).

## 3. Protocolo v2 (extiende el v1)

Igual que v1 en I/O e identificación. Cambios:

- FC04 `242` (versión de protocolo) = `0x0002`.
- FC04 `246` (capacidades): bit 6 = estado seguro configurable, bit 7 = configuración bloqueada por sketch.
- FC04 `247` (estado + ID): estado `1 configurado`, `5 de fábrica` (nuevo), `6 bloqueado por sketch` (nuevo).
- Registros de configuración 230..236, antes reservados:

```text
224 Clave                 0xA55A (habilita una escritura; se borra sola tras el comando)
225 ID nuevo              1..246
226 Baudrate (código)     0=9600 1=19200 2=38400 3=57600 4=115200
227 Comando               1 guardar ID · 2 guardar baud · 3 de fábrica · 4 reiniciar
                          5 guardar todo (ID, baud, trama, estado seguro, vínculo) — nuevo en v2
228 Parámetro del comando
229 Resultado             0 ok · 1 clave errónea · 2 fuera de rango · 3 bloqueado por sketch · 4 FRAM
230 Trama (código)        0=8N1 1=8E1 2=8O1
231 Estado seguro: valor  bit n = Qn en estado seguro
232 Estado seguro: mantener  bit n = 1 → Qn conserva su último valor
233 Tiempo de vigilancia  en 100 ms, 10..50 (1,0 a 5,0 s; mínimo 1 s por el ciclo del bus, alpha.12 §8)
234 Banderas (solo lectura)  bit0 configurado · bit1 vinculado · bit2 bloqueado por sketch
235 Vínculo, parte alta   CRC del UID del maestro (lo escribe el maestro con el comando 5)
236 Vínculo, parte baja
237..239 reservados
```

Enums y valores amplios ocupan un registro cada uno: el bus es lento pero esto se escribe una sola vez, y deja el mapa
legible desde cualquier herramienta Modbus.

**Bloqueado por sketch**: si el sketch fija la configuración (`begin(id, baud, trama)` con parámetros), el esclavo
responde a 225..233 con resultado 3 y no cambia nada. El maestro lo detecta por 246/234 y muestra los valores en solo
lectura.

## 4. Fábrica, alta y barrido

**Valores de fábrica** del sketch de esclavo: ID **247**, 115200, 8N1 (los del Backplane), estado seguro «todo OFF»,
vigilancia 1 s, sin vínculo. El 247 queda reservado para módulos sin configurar; los asignados usan 1..246.

**Volver a fábrica** en el esclavo: mantener ESC + OK durante 3 s al encenderlo (en módulos sin pantalla, un puente o
botón definido por el modelo). Cruce con `JW_MatrixButtons` (matriz 2 × 3): ESC y OK están en la
fila 1 (GPIO 2), así que se leen juntas sin teclas fantasma, y GPIO 2 en alto al arrancar no cambia el modo de
arranque. **No** se usan teclas de la fila 0 (◄ ▲ ►, GPIO 12): GPIO 12 es el pin de arranque MTDI y en alto al encender
cambia la tensión de la flash. La combinación se lee después de iniciar el escáner de la botonera y debe mantenerse 3 s.

**Agregar módulo** (Runtime del maestro, conectado solo el nuevo):

1. El maestro pausa el ciclo normal y consulta el ID 247 con la trama de fábrica (FC04 240..255).
2. Muestra la ficha: modelo, firmware, DI/DO, MAC, estado.
3. Si está bloqueado por sketch: muestra su ID y trama, y lo agrega tal cual (si su trama no es la del bus, avisa).
4. Si no: el usuario elige ID (propone el primer libre), estado seguro por salida (OFF / ON / mantener) y vigilancia.
   Baud y trama son los del bus (el bus es del maestro, como en el Backplane).
5. El maestro escribe 224..236 y el comando 5; el esclavo guarda en FRAM, responde y cambia de ID y trama.
6. El maestro verifica en el ID nuevo con la trama del bus (firma, MAC, banderas) y guarda el vínculo en su FRAM.

**Buscar módulos**: barre 1..246 con la trama del bus (≈ 5 s con 20 ms de espera por ID). Para cada respuesta muestra
ID, modelo, estado y si está vinculado a este maestro, a otro o a ninguno. Desde la lista se puede vincular uno
existente sin reconfigurarlo.

## 5. Operación en el maestro

- Tabla de vínculos en la FRAM del Runtime (`0x1100`, 7 × 16 B): ID, producto, CRC del UID, DI/DO, estado seguro y
  banderas. Hasta 7 módulos, como el Backplane.
- La tarea del Runtime lleva el bus con el maestro cooperativo de `JWPLC_ModbusRTU`: por módulo FC15 → FC01 → FC02,
  uno por vuelta; 3 fallos = fuera de línea, sus entradas a 0; los caídos se sondean cada 1 s.
- Direcciones en el programa con **prefijo de módulo**: `1.I1`…`1.I8` y `1.Q1`…`1.Q8` para el módulo 1, hasta `7.I8`
  y `7.Q8`. Las locales siguen como I1…I8 y Q1…Q8. Codificación: las entradas remotas usan las fuentes 0x40..0x77
  (módulo = (s − 0x40) / 8 + 1) y el programa agrega 7 × 8 destinos para las Q remotas (56 B; la imagen sigue entrando
  en su ranura de 1536 B). Imagen de programa versión 2, que también lee la versión 1.
- Al arrancar se verifica cada vínculo (firma y UID). Si falta o es otro equipo: aviso con código ERR (`X1`…`X7` = módulo
  sin respuesta, `XU1`… = equipo distinto) y evento. No se reconfigura nada automáticamente.
- El estado seguro lo aplica el **esclavo** por su cuenta al vencer la vigilancia; el maestro solo lo configura y lo
  muestra.

## 6. Pantallas (Configuración > Expansión)

```text
EXPANSION
  Bus RS-485          115200 8N1   < >
  Modulo 1  ID 1      JWPLC Basic  en linea
  Modulo 2  ID 2      JWPLC Basic  sin respuesta
  + Agregar modulo    (conectar solo el nuevo)
  Buscar modulos      barrido 1..246

MODULO 1 (detalle)
  Modelo / firmware / MAC / DI-DO
  Direcciones         1.I1-1.I8  1.Q1-1.Q8
  Estado seguro       1.Q1 OFF  1.Q2 mantener ...   (solo lectura si está bloqueado por sketch)
  Vigilancia          1,0 s
  Quitar modulo
```

En el editor, el selector de fuentes agrega 1.I1… y la columna de salidas 1.Q1… solo para módulos vinculados; Diagnóstico muestra por módulo
en línea / fuera de línea, fallos y tiempo de ciclo del bus.

## 7. Esclavo

- Librería `JWPLC_Expansion` con el objeto `JWPLC_ExpansionSlave`:
  - `JWPLC_ExpansionSlave.begin()`: configuración de su FRAM o de fábrica (configurable desde el maestro).
  - `JWPLC_ExpansionSlave.begin(3, 115200, SERIAL_8N1)`: configuración fija desde el sketch (bloqueado).
- Ejemplo `Expansion_Esclavo_Basic` (JWPLC Basic como DI8/DO8). La pantalla RUN muestra «Esclavo ID 3» o
  «Esclavo de fabrica» en el título.
- FRAM del esclavo: la estructura de alpha.3 (`JwplcRemoteIoConfigV1`) pasa a v2 con trama, estado seguro, vigilancia,
  vínculo y CRC.

## 8. Decisiones del propietario (01/11)

1. ID 247 de fábrica; 1..246 para los módulos asignados. **Sí.**
2. Estado seguro por salida: **OFF, ON y mantener.**
3. Volver a fábrica con ESC + OK al encender: **sí**, sin cruzarse con `JW_MatrixButtons` (ver §4).
4. I/Q remotas **con prefijo de módulo** (`1.I1`).

Además: las filas y baldosas vuelven a sprites de 4 bits (los de 16 bits no mejoraron el corte en la placa y usaban
23 KB más de RAM). Runtime v0.1.3.

## 9. Validación prevista

- JWPLC Sim: maestro y esclavo como dos PLC en el bus RS-485 multinodo del simulador (ARCHITECTURE §7.3, gate G4
  RS485_MULTINODE), con ruido inyectado (`rs485-noise`) para los fallos.
- Física: dos JWPLC Basic; alta de módulo, barrido, desconexión (entradas a 0, aviso), estado seguro en el esclavo al
  cortar el cable, esclavo bloqueado por sketch, volver a fábrica.

## 10. Esclavo en el simulador (01/11)

[ejecutado] Maestro de prueba y `Expansion_Esclavo_Basic` como dos PLC en el bus RS-485 de JWPLC Sim (115200 8N1):

| Prueba | Resultado |
|---|---|
| Identificación de fábrica (FC04 240..255 en el ID 247) | firma `JWPL`, protocolo 2, producto 1, DI/DO 8/8, estado 5 (de fábrica) |
| Comando 5 «guardar todo» (ID 5, estado seguro Q1 ON / Q2 mantener, vigilancia 1 s, vínculo) | responde en el ID 5 con estado 1, banderas configurado + vinculado; el 247 deja de responder |
| Clave errónea | resultado 1, nada cambia |
| Salidas Q2+Q3 escritas cada 100 ms y luego el maestro calla | tras 1 s las salidas pasan a Q1+Q2 (`0x03`): Q1 ON, Q2 mantenida, Q3 OFF |
| Reinicio con la misma FRAM | vuelve en el ID 5 |
| ESC + OK mantenidas 3 s al encender | vuelve a fábrica (ID 247, sin vínculo) |
| Sketch con `begin(3, 115200, SERIAL_8N1)` | estado 6 (bloqueado), capacidad bit 7; el comando 5 responde 3 y la configuración no cambia |

## 11. Corte de imagen (tearing) en la TFT

En la placa v2.0 el pin TE (FMARK) del ST7789 no tiene un GPIO libre (propietario, 01/11), así que el refresco no se puede
sincronizar con el panel. Las filas y baldosas se suben de una sola vez desde sprites de 4 bits, que es lo que se puede
hacer por software. Sugerencia para la próxima revisión del Top: llevar TE a un GPIO de entrada libre.
