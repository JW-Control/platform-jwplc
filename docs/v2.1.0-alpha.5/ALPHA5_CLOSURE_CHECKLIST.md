# JWPLC v2.1.0-alpha.5 - Checklist de cierre

Fecha: 2026-08-24

## Identificación

- [x] Versión objetivo: `v2.1.0-alpha.5`.
- [x] Rama de trabajo:
  `v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`.
- [x] Rama objetivo de integración: `release/v2.1.x`.
- [x] Objetivo:
  recuperar rendimiento mediante precompilación manteniendo estabilidad,
  compatibilidad Arduino IDE y todos los periféricos del autoload normal.

Commit técnico/documental de referencia antes de este checklist:

`f6d935f9f8af8beac82a9d202baf90bd6fb4be66`

---

## 1. Compatibilidad de librerías precompiladas

- [x] Auditor de símbolos externos `jwplc...` corregido.
- [x] Se detectan símbolos snake_case y camelCase.
- [x] Bridge GPIO limitado exclusivamente a:
  - `jwplc_pinMode`;
  - `jwplc_digitalWrite`;
  - `jwplc_digitalRead`.
- [x] No se introdujeron bridges genéricos de SPI.
- [x] No se introdujeron bridges genéricos de I2C.
- [x] No se introdujeron shims del runtime para recuperar rendimiento.
- [x] Generic ESP32 aporta `cores/esp32/jwplc-gpio-compat.c`.
- [x] Auditoría global final completada.
- [x] Archives auditados: 12.
- [x] Dependencias `jwplc...` bloqueantes: 0.
- [x] Resultado global:
  `PASS BRIDGE-COMPATIBLE`.

Marcadores:

`ALPHA5_PRECOMPILED_GLOBAL_AUDIT=PASS`

`GENERIC_GPIO_BRIDGE_POLICY=ENFORCED`

`BLOCKING_JWPLC_SYMBOLS=0`

Fuente:

`docs/v2.1.0-alpha.5/PRECOMPILED_GLOBAL_AUDIT_FINAL_20260824.md`

---

## 2. Estado final de librerías

### Precompiladas / adoptadas

- [x] Adafruit ST7735/ST7789.
- [x] Adafruit GFX.
- [x] Adafruit BusIO.
- [x] Wire.
- [x] SPI.
- [x] FS.
- [x] SD.
- [x] JW_FRAM.
- [x] JW_SD.
- [x] JWPLC Ethernet W5x00 Backend.
- [x] JWPLC_ModbusRTU.
- [x] JW_MatrixButtons.

### Source fallback deliberado

- [x] `JWPLC_Display`: 2 TUs.
- [x] `JW_RTC`: 1 TU.

Resultado final de recuperación:

`PRECOMPILED_RECOVERED_TUS=21/24`

`SOURCE_FALLBACK_TUS=3/24`

No se considera un fallo que Display y RTC permanezcan desde fuente:
sus dependencias externas al runtime hacen preferible conservar compatibilidad
antes que introducir shims nuevos.

---

## 3. JW_MatrixButtons

- [x] Archive generado.
- [x] ABI auditado.
- [x] Sólo utiliza los tres símbolos GPIO permitidos.
- [x] Generic ESP32 link PASS.
- [x] JWPLC Basic link PASS.
- [x] JWPLC Basic Core link PASS.
- [x] Gate físico de seis botones PASS.
- [x] Arduino IDE utiliza el archive precompilado.
- [x] Adoptada con `precompiled=full`.

Archive:

`JWPLC/2.1.0/libraries/JW_MatrixButtons/src/esp32/libJW_MatrixButtons.a`

SHA-256:

`55be8d7791ddad79d613dbb199c10a504de0f20cdf3330b6679a35dd64e25c81`

Resultado:

`JW_MATRIXBUTTONS_PRECOMPILED=ADOPTED`

---

## 4. Arquitectura del core

Fuente canónica:

`JWPLC/2.1.0/cores/jwcontrol/`

Stub de Basic normal:

`JWPLC/2.1.0/cores/jwcontrol_precompiled_stub/`

Archive:

`JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a`

- [x] `jwcontrol` es la única fuente funcional del core JWPLC.
- [x] El stub no contiene lógica funcional.
- [x] `jwplcbasic` usa stub + `core.a`.
- [x] `jwplcbasiccore` conserva compilación desde fuente.
- [x] Se eliminó el concepto funcional `jwcontrol_p2`.
- [x] Generador del core normalizado.
- [x] Verificador del core normalizado.
- [x] Basic precompiled core PASS.
- [x] Basic Core source control PASS.
- [x] Arquitectura compatible con Arduino IDE.

SHA-256 del archive validado:

`7c2c0149fc19e5f363a46d49c9a805db268b2ab72ae0a4ebf2b679e781a2d669`

Reproducibilidad:

- [x] 62/64 objetos determinísticos byte a byte.
- [x] 2/64 objetos dependen de `__DATE__/__TIME__`.
- [x] Reproducibilidad semántica PASS.
- [x] Bit-for-bit global no exigido por diseño actual.

---

## 5. Display y coexistencia SPI

- [x] Se identificó contención durante inicialización tardía del TFT.
- [x] TFT pasa a inicializarse antes de `setup()`.
- [x] Callback posterior es idempotente mediante estado `g_tftReady`.
- [x] No se eliminaron delays requeridos por ST7789.
- [x] No se eliminó el mutex SPI.
- [x] No se ampliaron artificialmente timeouts.
- [x] Ethernet sin Link PASS.
- [x] Ethernet con IP estática PASS.
- [x] Ethernet con DHCP real PASS.
- [x] FRAM sin fallos durante gates finales SPI.
- [x] microSD sin fallos durante gates finales SPI.
- [x] Timeout SPI Ethernet: 0.

Resultado:

`ALPHA5_SPI_STARTUP_COEXISTENCE=PASS`

`ALPHA5_TFT_PRESETUP=ADOPTED`

Fuente:

`docs/v2.1.0-alpha.5/SPI_STARTUP_TFT_PRESETUP_20260824.md`

---

## 6. Autoload y periféricos

Alpha5 no obtiene rendimiento retirando periféricos del autoload normal.

Se conservan:

- [x] Display.
- [x] Ethernet / W5500.
- [x] microSD.
- [x] FRAM.
- [x] RTC.
- [x] Botonera.
- [x] RS-485.
- [x] Modbus RTU.
- [x] TCA / entradas y salidas.
- [x] Mutex SPI global.

Resultado:

`ALPHA5_AUTOLOAD_PERIPHERALS=PRESERVED`

---

## 7. Gates de compilación y compatibilidad

- [x] JWPLC Basic compila.
- [x] JWPLC Basic Core compila.
- [x] Generic ESP32 pasa gates de compatibilidad.
- [x] Arduino CLI validado.
- [x] Arduino IDE 2.x validado.
- [x] Gate integral heredado de Alpha4 compila sobre el HEAD final Alpha5.

Gate compile-only final:

`tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate`

Resultado:

`ALPHA5_FINAL_LOCAL_GATE_COMPILE=PASS`

Arduino IDE:

`ALPHA5_ARDUINO_IDE_COMPILE=PASS`

El compile del gate integral produjo:

- aplicación: 416353 bytes;
- uso APP: 10 %;
- máximo APP: 4063232 bytes;
- RAM global: 27660 bytes;
- uso RAM: 8 %.

---

## 8. Evidencia física

Durante Alpha5 se ejecutaron gates físicos dirigidos sobre las áreas
funcionalmente modificadas.

- [x] JW_MatrixButtons física.
- [x] microSD física.
- [x] GFX / TFT físico.
- [x] Display inicializado antes de setup.
- [x] Ethernet W5500 físico.
- [x] Ethernet Link OFF.
- [x] Ethernet Link ON.
- [x] Ethernet con IP estática.
- [x] Ethernet con DHCP.
- [x] FRAM durante coexistencia SPI.
- [x] microSD durante coexistencia SPI.
- [x] Redraw TFT durante coexistencia SPI.

Las funciones no modificadas conservan además los gates físicos de Alpha4:

- RTC;
- 8 entradas digitales;
- 8 salidas/relés;
- RS-485;
- Modbus RTU FC03;
- Modbus RTU FC06.

No se realizó una repetición física integral del sketch
`06_alpha4_local_physical_gate` sobre el HEAD documental final porque no había
hardware JWPLC Basic disponible durante el cierre.

Estado:

`ALPHA5_FINAL_LOCAL_PHYSICAL_RERUN=NOT_EXECUTED`

Motivo:

`NO_HARDWARE_PRESENT_AT_CLOSURE`

Esto no se clasifica como FAIL porque:

1. las áreas funcionalmente modificadas cuentan con validación física;
2. desde la última modificación funcional hasta el HEAD de cierre sólo hubo
   cambios de documentación;
3. el gate físico integral sí fue recompilado exitosamente sobre el estado
   final del package.

Se recomienda repetir el gate integral cuando exista hardware disponible,
preferiblemente durante la validación del PreRelease publicado.

---

## 9. Rendimiento final Alpha5

Run final principal:

`20260824_134822`

### JWPLC Basic

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 54.594 s | 8 |
| managed warm no-change | 24.804 s | 1 |
| managed warm touch | 23.760 s | 1 |
| explicit cold | 55.387 s | 8 |
| explicit warm no-change | 22.462 s | 1 |
| explicit warm touch | 22.219 s | 1 |

### JWPLC Basic Core

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 60.717 s | 71 |
| managed warm no-change | 20.934 s | 1 |
| managed warm touch | 21.085 s | 1 |
| explicit cold | 58.617 s | 71 |
| explicit warm no-change | 21.393 s | 1 |
| explicit warm touch | 21.221 s | 1 |

Resultado:

`ALPHA5_FINAL_BENCHMARK=PASS`

---

## 10. Comparación Alpha4 vs Alpha5

Comparación principal en el mismo PC y con metodología cold comparable:

| Estado | Tiempo | TUs |
|---|---:|---:|
| Alpha4 P6 | 67.322 s | 12 |
| Alpha5 final | 55.387 s | 8 |

Mejora Alpha5:

- 11.935 s menos;
- 17.73 % menos tiempo;
- 12 -> 8 TUs;
- 33.33 % menos TUs compiladas.

Resultado:

`ALPHA5_VS_ALPHA4_SAME_HOST=IMPROVED`

Alpha4 P8:

`59.901 s / 5 TUs`

se conserva sólo como referencia histórica porque fue medido en otro host.

Fuente:

`docs/v2.1.0-alpha.5/BUILD_SPEED_COMPARISON_ALPHA4_ALPHA5_FINAL_20260824.md`

---

## 11. Laptop de validación

Se realizaron pruebas adicionales en una laptop con Arduino CLI 1.5.1.

Comparación controlada Equilibrado:

| Fase JWPLC Basic | AC | Batería |
|---|---:|---:|
| managed cold | 66.475 s | 87.032 s |
| explicit cold | 76.694 s | 87.533 s |
| explicit warm no-change | 8.630 s | 11.286 s |
| explicit warm touch | 8.749 s | 11.556 s |

Conclusión:

- el rendimiento del host afecta materialmente el tiempo;
- en batería existe penalización medible;
- el número de TUs de Basic permanece en 8;
- para benchmarks formales en laptop se recomienda cargador conectado.

Resultado:

`ALPHA5_LAPTOP_POWER_COMPARISON=PASS`

Fuente:

`docs/v2.1.0-alpha.5/LAPTOP_POWER_PROFILE_BENCHMARK_20260824.md`

---

## 12. App-only

- [x] App-only permanece funcional como herramienta de desarrollo.
- [x] No se adopta como upload normal.
- [x] Full upload permanece como ruta por defecto y segura.
- [x] Alpha5 no modifica la receta base de upload de `platform.txt`.
- [x] No se requiere repetir la prueba física de app-only en Alpha5.
- [x] No se añade un menú público UploadMode.

Resultado:

`APP_ONLY=VALIDATED_DEVELOPMENT_TOOL`

`APP_ONLY_DEFAULT_UPLOAD=NO`

`FULL_UPLOAD_DEFAULT=YES`

---

## 13. Bootloader

- [x] Bootloader precompilado evaluado previamente.
- [x] Beneficio de rendimiento pequeño e inconsistente.
- [x] No se adopta `bootloader.bin` precompilado.
- [x] No se publica `bootloader.bin` como artefacto definitivo.
- [x] La generación continúa desde el ELF del SDK.
- [x] Alpha5 no modifica la receta base de bootloader de `platform.txt`.

Resultado:

`BOOTLOADER_PRECOMPILED=NOT_ADOPTED`

`BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC`

---

## 14. Particionado y configuración

JWPLC Basic conserva:

- [x] MCU: ESP32.
- [x] CPU: 240 MHz.
- [x] Flash: 4 MB.
- [x] Flash frequency actual: 40 MHz.
- [x] Flash image mode: DIO.
- [x] `build.boot=qio`.
- [x] Bootloader address: `0x1000`.
- [x] Upload speed: 921600.
- [x] Partición: `jwplc_max_app_4mb`.
- [x] APP máxima: 4063232 bytes.
- [x] Coredump: 64 KiB.
- [x] SPIFFS no forma parte del perfil JWPLC Basic actual.

La configuración anterior representa el perfil actual validado.

No se declara como configuración universal para futuras revisiones.

Resultado:

`CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE`

`FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING`

`FLASHFREQ_REEVALUATION_ALPHA5=OUT_OF_SCOPE`

Fuente:

`docs/v2.1.0-alpha.5/UPLOAD_BOOTLOADER_CONFIGURATION_CONCLUSION_20260824.md`

---

## 15. Decisiones de alcance

- [x] No asumir OpenPLC integrado.
- [x] No definir OTA en Alpha5.
- [x] La presencia de `otadata` no implica una política OTA.
- [x] No abrir migración a FlashFreq 80 MHz en Alpha5.
- [x] No publicar un bootloader.bin definitivo.
- [x] No retirar periféricos por velocidad.
- [x] No romper APIs públicas para recuperar precompilación.
- [x] No introducir SPI/I2C shims genéricos.
- [x] No mezclar arquitectura ESP32-S3 con este cierre.
- [x] No avanzar a otra alpha antes de cerrar documentalmente Alpha5.

---

## 16. Documentación técnica requerida

- [x] Tabla final de tiempos.
- [x] Comparación Alpha4 vs Alpha5.
- [x] Benchmark en segundo equipo.
- [x] Conclusión app-only.
- [x] Conclusión bootloader.
- [x] Configuración final actual / pendiente universal explícito.
- [x] Auditoría global final.
- [x] Decisión MatrixButtons.
- [x] Decisión Display source fallback.
- [x] Decisión RTC source fallback.
- [x] Reproducibilidad del core documentada.
- [x] Coexistencia SPI / TFT documentada.
- [x] Arduino IDE validado.
- [x] Checklist Alpha5 preparado.

Pendiente de preparar:

- [x] `PULL_REQUEST.md` en español.
- [x] `PRE_RELEASE.md` en español.

---

## 17. Pendientes de integración y publicación

- [x] Revisar diff final Alpha4 -> Alpha5.
- [x] Preparar PULL_REQUEST.md.
- [x] Preparar PRE_RELEASE.md.
- [ ] Commit de documentación final.
- [ ] Push final de la rama.
- [ ] Crear PR hacia `release/v2.1.x`.
- [ ] Revisar CI del PR.
- [ ] Hacer merge si todos los gates son satisfactorios.
- [ ] Preparar ZIP de `v2.1.0-alpha.5`.
- [ ] Calcular tamaño y SHA-256 del ZIP.
- [ ] Actualizar índice dev correspondiente.
- [ ] Crear tag de Alpha5.
- [ ] Crear GitHub PreRelease.
- [ ] Adjuntar ZIP.
- [ ] Validar instalación por Boards Manager.
- [ ] Validar compilación desde instalación publicada.
- [ ] Validar upload desde instalación publicada.
- [ ] Repetir gate físico integral cuando haya hardware disponible.

---

## 18. Criterio de cierre técnico

La fase técnica de compatibilidad/precompilación de Alpha5 se considera
cerrada.

Se cuenta con:

- arquitectura de core normalizada;
- bridge GPIO limitado;
- auditoría global sin bloqueantes;
- recuperación segura de 21/24 TUs candidatas;
- 3/24 TUs mantenidas deliberadamente desde fuente;
- gates Generic, Basic y Basic Core;
- gates físicos dirigidos sobre las áreas modificadas;
- benchmark final;
- comparación Alpha4 vs Alpha5;
- validación en segundo equipo;
- compilación desde Arduino CLI;
- compilación desde Arduino IDE;
- decisiones explícitas de app-only, bootloader y configuración.

El único gate físico no repetido sobre el HEAD documental final es la
regresión integral local por ausencia de hardware durante el cierre. No se
considera bloqueante para preparar el PR porque no hubo cambios funcionales
posteriores a los gates físicos específicos.

Estado:

`ALPHA5_TECHNICAL_CLOSURE=PASS`

`ALPHA5_READY_FOR_PR_DOCUMENTATION=YES`

`ALPHA5_RELEASE_PUBLICATION=PENDING`


