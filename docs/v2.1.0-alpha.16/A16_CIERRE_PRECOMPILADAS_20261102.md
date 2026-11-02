# A16 cierre — librerías precompiladas regeneradas (02/11/2026)

```text
A16_CIERRE_A_REGENERADOS=PASS
A16_CIERRE_ABI=PASS
A16_CIERRE_EJEMPLOS_ESP32=PASS (138/138)
A16_CIERRE_MATRIZ_SIM=PASS (138/138)
A16_CIERRE_BENCHMARK_PC=PENDING (propietario)
```

## 1. Qué se regeneró

Durante alpha.16 tres librerías precompiladas se trabajaron en modo fuente (regla del 01/11). Al cierre se
regeneran sus `.a` desde el árbol de `v2.1.0-alpha.16/JWPLC-Sim/main` (`d850f40`) y vuelven a `precompiled=full`:

| Librería | Versión | Objetos en el `.a` | Bytes antes → después |
|---|---|---|---|
| `JWPLC_TFT` | 0.5.0 | `JWPLC_TFT`, `JWPLC_TFT_Font16`, `_Font26`, `_Font7Seg48`, `JWPLC_TFT_Mirror`, `TFT_eSPI` | 1 733 562 → 1 662 196 |
| `JWPLC_Display` | 1.1.0 | `JWPLC_Display`, `_H3E1_Profile`, `JWPLC_IdleScreen`, `JWPLC_UI`, `_UI_API`, `_UI_EventLog`, `_UI_Format`, `_UI_Menu`, `_UI_Pages`, `_UI_PixelMap` | 1 370 778 → 1 671 314 |
| `JW_MatrixButtons` | 1.0.6 | `JW_MatrixButtons` | 129 506 → 133 812 |

`[ejecutado]` con `tools/alpha16/regenerate_precompiled.sh build --install` (nuevo; reemplaza a los dos scripts
de alpha.15 porque TFT_eSPI ya va dentro de `JWPLC_TFT/src` con el DISPON diferido, sin parche aparte):

- arduino-cli 1.1.1, `jwplc_local:esp32:jwplcbasic`, toolchain `esp-x32` 2601 (arduino-esp32 3.3.8).
- Sketch sonda con `JWPLC_Display.h` y `JW_MatrixButtons.h`; las tres librerías compiladas desde copias sin
  `precompiled=full`; **0 advertencias y 0 errores**.
- ABI frente a los `.a` anteriores: **0 símbolos globales fuertes eliminados** (TFT +76, Display +43,
  MatrixButtons +5) y **0 símbolos importados por los demás `.a` del package sin definir**
  (`ABI_COMPAT=PASS`).
- SHA-256: `libJWPLC_TFT.a` `acd6b97c…`, `libJWPLC_Display.a` `118d25a5…`, `libJW_MatrixButtons.a` `c6cbcd35…`.

Los `.cpp` siguen en el repo junto al `.a` (igual que en alpha.15): con `precompiled=full` Arduino no los
compila. JWPLC Sim compila siempre desde fuente y no usa los `.a`.

## 2. Librerías nuevas: quedan en fuente

`JWPLC_UI_Widgets` 0.5.0, `JWPLC_Runtime` 0.2.0, `JWPLC_Expansion` 0.1.0 y `JWPLC_WebScreen` 0.1.0 nacieron en
esta alpha y se publican **en modo fuente**: sólo se compilan si el sketch las incluye, todavía cambian con la
validación en placa y no tienen `.a` previo contra el cual verificar ABI. Se precompilan cuando se estabilicen.

## 3. Ejemplos en ESP32 real y matriz del simulador

- `[ejecutado]` compilación ESP32 real (`jwplc_local:esp32:jwplcbasic`, ctags real 5.8-arduino11) de los **138**
  ejemplos de las librerías `JWPLC_*` y `JW_*` con los `.a` nuevos instalados: **138/138 compilan, 0 advertencias**
  (incluye `JWPLC_LogicRuntime*`, todavía en el package).
- `[ejecutado]` JWPLC Sim, `jwsim-cli matrix` completa sobre el mismo árbol: **138 ejemplos; 138 compilan, 138
  corren, 138 pasan su prueba**; 0 clasificados, 0 sin prueba.
- El CI del PR hacia `release/v2.1.x` (`ci-jwplc-package-smoke.yml`) vuelve a compilar los ejemplos críticos y
  corre `SIM_REGRESSION` con jwplc-sim `a81f302`.

## 4. Benchmark en la PC del propietario

Pendiente: compilar en Arduino IDE 2.3.10 un ejemplo con pantalla (p. ej. `JWPLC_Display/05.HMI_Plantilla_Caldera`)
con el package de esta rama instalado como `jwplc_local`, en limpio y recompilando, y anotar los tiempos aquí.
Referencia de alpha.15: 18 s → 9 s recompilando con el IDE 2.3.10.

## 5. Cómo compilar ESP32 en la nube sin downloads.arduino.cc

El proxy de la nube responde 403 a `downloads.arduino.cc`. Funciona así:

1. arduino-cli desde sus releases de GitHub; `esp-x32`, `esp32-libs` 3.3.8 y `esptool_py` desde los releases de
   GitHub de Espressif, descomprimidos a mano en `data/packages/jwplc/tools/<tool>/<versión>`.
2. `package_index.json` y `library_index.json` vacíos en el directorio de datos.
3. `ctags` 5.8-arduino11 desde `github.com/arduino/ctags/releases`, pasado con
   `--build-property runtime.tools.ctags.path=<dir>` (o `CTAGS_DIR=<dir>` en el script).
4. Plataforma: enlace `data/packages/jwplc_local/hardware/esp32/2.1.0-dev` → `JWPLC/2.1.0` del repo.
