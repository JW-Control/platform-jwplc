# P3 determinista - resultado 2026-08-09

## Contexto

Se repitio la validacion de P3 despues de fijar las dependencias externas del package:

- Adafruit ST7735/ST7789: copia bundled del package.
- Adafruit GFX: copia bundled del package.
- Adafruit BusIO: copia bundled del package.
- Ethernet: `JWPLC Ethernet W5x00 Backend 2.0.2` vendorizado desde el tag oficial `arduino-libraries/Ethernet` 2.0.2.

El primer cold fuente del run `20260809_190321` termino correctamente, pero el script original produjo un falso negativo al contar 31 TUs de Display por un regex codicioso. La inspeccion del build confirma exactamente dos objetos fuente de Display: `JWPLC_Display.cpp.o` y `JWPLC_IdleScreen.cpp.o`.

La reanudacion genero `libJWPLC_Display.a` usando esos dos objetos deterministas y ejecuto un segundo cold P3 sin repetir el primero.

## Resultado validado

| Metrica | Fuente P1+P2 | P3 determinista |
|---|---:|---:|
| Tiempo cold | N/D por falso negativo del parser | 101.677 s |
| Compilaciones | 34 | 32 |
| Preprocesados `-E` | 49 | 49 |
| TUs Display desde fuente | 2 | 0 |
| App bytes | 406016 | 406016 |

Validaciones estructurales P3:

- `JWPLC_Display.cpp.o` extraido del archive: OK.
- `JWPLC_IdleScreen.cpp.o` extraido del archive: OK.
- Objetos externos comunes: 30.
- SHA distintos en objetos externos: 0.
- Objetos solo fuente: 0.
- Objetos solo P3: 0.
- Diferencias de seleccion de librerias: 0.
- Adafruit bundled: OK.
- Ethernet W5x00 bundled: OK.

## Compilaciones restantes en P3

El cold P3 determinista ejecuta 32 compilaciones:

| Bloque | TUs |
|---|---:|
| JWPLC Ethernet W5x00 Backend 2.0.2 | 8 |
| Adafruit ST7735/ST7789 | 4 |
| Adafruit GFX | 4 |
| Adafruit BusIO | 4 |
| SD | 3 |
| FS | 2 |
| sketch | 1 |
| JWPLC_GlobalPeripherals | 1 |
| Wire | 1 |
| SPI | 1 |
| JWPLC_Ethernet | 1 |
| JWPLC_RS485 | 1 |
| core P2 stub | 1 |
| **Total** | **32** |

## Discovery restante

Las 49 pasadas `-E` son identicas en el build fuente y P3. Desglose observado:

| Origen | Pasadas `-E` |
|---|---:|
| JWPLC_GlobalPeripherals.cpp | 11 |
| sketch `01_empty` | 8 |
| JWPLC Ethernet W5x00 Backend | 8 |
| Adafruit ST7735/ST7789 | 4 |
| Adafruit GFX | 4 |
| Adafruit BusIO | 4 |
| SD | 3 |
| FS | 2 |
| Wire | 1 |
| SPI | 1 |
| JWPLC_Ethernet | 1 |
| JWPLC_RS485 | 1 |
| sketch merged temporal de Arduino | 1 |
| **Total** | **49** |

## Conclusion P3

P3 sigue siendo estructuralmente valido y mantiene exactamente el mismo tamano de aplicacion, pero con las dependencias bundled actuales su efecto se limita a evitar dos compilaciones de Display. Ya no reduce el trabajo de library discovery.

Por tanto, P3 se mantiene como parte de la pila valida, pero el siguiente esfuerzo no debe volver a centrarse en Display ni en GlobalPeripherals.

## Direccion P5

El mayor bloque restante ya es reproducible y esta bajo control del package:

1. Adafruit bundled: 12 TUs.
2. Ethernet W5x00 bundled: 8 TUs.

Para conservar estabilidad y aislar el impacto, P5 se divide en pilotos incrementales:

- **P5A:** precompilacion de las tres librerias Adafruit bundled como bloque coherente.
- **P5B:** precompilacion del backend Ethernet W5x00 2.0.2 bundled, solo despues de cerrar P5A.

No se mezclara P5 con P4 ni se volvera a precompilar GlobalPeripherals en este punto.

## Pendientes relacionados

- Corregir el contador de TUs en `Run-JWPLCP3DeterministicBaseline.ps1` para inspeccionar el operando fuente real y no rutas `-I`.
- Mantener `libJWPLC_Display.a` determinista activo para los siguientes pilotos.
- Obtener un cold fuente/P3 con tiempo completo en una corrida futura si se requiere comparacion estadistica directa; el tiempo del primer cold `20260809_190321` no se usara como dato oficial porque el Stopwatch se perdio tras el falso negativo.
