# Alpha13-G7C — Completar A13-009 y A13-013 en FRAM

**Estado:** CANDIDATE_AND_OFFLINE_RUNNER_VERSIONED_NOT_EXECUTED_WINDOWS
**Fecha:** 2026-10-11
**Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`

G7 técnico está `CLOSED_PASS`; no se revierte ni repite.
G7C se introduce para cerrar dos hallazgos de la auditoría
canónica que NO se resolvieron con los null guards y checksum G7.

## Contrato exacto

- **A13-009** (`JW_FRAM.cpp`, `readString()`):
  la longitud se lee con `read(addr,&len,sizeof(len))`, que
  distingue fallo de bus frente a longitud cero legítima.
  La lectura del payload se mantiene; `String::reserve()`
  ahora debe devolver `true` antes de construir una cadena
  no vacía. Además, el string vacío es válido si el byte de
  longitud está en la última dirección FRAM.
  No se altera representación `[uint8_t length][payload]`,
  longitud máxima ni API pública.
- **A13-013** (`JW_FRAM.cpp/.h`): retirar solo los guards
  muertos `if (_spi_dev) delete _spi_dev` de ambos
  constructores (miembro declarado con inicialización
  `nullptr`) y las dos sobrecargas **privadas** no usadas
  `debugPrint(const String&)` y `debugPrint(const char*)`.
  Conservar `debugPrint(const __FlashStringHelper*)`,
  `debugPrintValue()`, API de usuario y SPI/autoload.

## Artefactos preparados

```text
CANDIDATE_CPP=tools/alpha13/candidates/g7c/JW_FRAM.cpp
CANDIDATE_H=tools/alpha13/candidates/g7c/JW_FRAM.h
SOURCE_TEST=tools/alpha13/gates/a13_g7c_source_selftest.py
OFFLINE_RUNNER=tools/alpha13/gates/a13_g7c_integrated_offline.py
NORMAL_PRODUCT_CPP=UNCHANGED
NORMAL_PRODUCT_H=UNCHANGED
NORMAL_PRODUCT_ARCHIVE=UNCHANGED
PHYSICAL_FIRMWARE=UNCHANGED
G7C_COMMIT_PRODUCT=NO
G7C_RELEASE_PR=NO
```

## Secuencia acordada — integrada, no destructiva

1. **Preflight**: rama/HEAD remoto, worktree limpio,
   Arduino CLI oficial 1.5.1, SHA igual al G7,
   core/TFT archives base, biblioteca `precompiled=full`.
   Detener sin tocar producto ante discrepancias.
2. **Fuente y escenarios**: el test reconstruye byte a byte
   el candidato desde el producto mediante SOLO los cambios
   permitidos. Test C++ nativo opcional extrae el método
   productivo real `JW_FRAM::readString()` y lo compila/
   ejecuta con `read()`, memoria y `reserve()` controlados.
   Comprueba 11 casos: fallo lectura longitud, vacío válido,
   payload normal, `maxLen`, límite interno, payload fuera
   de rango, fallo SPI payload, reserva fallida, vacío al
   final FRAM, longitud 127 exacta y cabecera fuera de rango.
   **Si no hay compilador nativo**, registrar
   `NATIVE_CPP=NOT_AVAILABLE_PENDING`, nunca PASS fingido.
3. **Reconstrucción y enlace**: reutilizar las funciones G7
   con overlay aislado, compilar `JW_FRAM.cpp` desde fuentes
   con BusIO bundled, crear `libJW_FRAM.a`, extraer miembro
   y comparar objeto por SHA. El enlace `-L/-lJW_FRAM`
   debe coincidir con el archive exacto. Compilar
   `GLOBAL_FRAM` y `CONFIG_BLOCK` contra ese archivo.
4. **Auditoría final offline**: hashes del producto antes/después
   idénticos, Git sin modificaciones ni staged files, logs
   separados y manifiesto `A13-G7C-OFFLINE-1`.
   El resultado es `CANDIDATE_BUILD_PASS_NATIVE_PASS`
   o `CANDIDATE_BUILD_PASS_NATIVE_PENDING`, **nunca**
   `G7C=CLOSED_PASS` por la sola compilación.
5. **Tras revisar la evidencia**: decidir validación runtime
   adicional y preparar adopción exacta de las dos fuentes
   y archive regenerado con rollback. No subir firmware ni
   repetir las escrituras FRAM de G7 salvo una nueva
   decisión explícita y justificada. Cerrar A13-009/013
   únicamente después de evidencia y Git productivo.

## No cambiar

```text
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
FRAM_AUTLOAD=UNCHANGED
G7_TECHNICAL_GATE=CLOSED_PASS
F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING
NO_PRODUCT_WRITE_DURING_OFFLINE=YES
NO_ARDUINO_UPLOAD_DURING_OFFLINE=YES
NO_AUTO_GIT_CLOSURE_DURING_OFFLINE=YES
A13_009=IN_PROGRESS_G7C
A13_013=IN_PROGRESS_G7C
NEXT_GATE=G7C_OFFLINE_INTEGRATED
```
