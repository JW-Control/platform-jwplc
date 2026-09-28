# Alpha14 — H3E.1B.2 — Candidate archive Display regenerado

Fecha: 2026-09-27

## Objetivo

Demostrar que el problema observado en H3E.1B.1 pertenece al archive histórico
de `JWPLC_Display` y no al uso de `precompiled=full` como estrategia.

H3E.1B.1 cerró:

```txt
Archive Alpha11
SYS_DISPLAY AVG = 19049 us
SYS_DISPLAY MAX = 23629 us

Source actual
SYS_DISPLAY AVG = 8575 us
SYS_DISPLAY MAX = 9840 us
```

El source actual fue además limpio en RTU/TCP/SD.

## Evidencia histórica

El archive actual:

```txt
SHA256=2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF
```

es exactamente el archive publicado en Alpha11.

En Alpha11 estaba documentado con:

```txt
DISPLAY_TUS=6
```

El source actual H3E.1B.2 contiene 7 TUs:

```txt
JWPLC_Display.cpp
JWPLC_Display_H3E1_Profile.cpp
JWPLC_IdleScreen.cpp
JWPLC_UI.cpp
JWPLC_UI_API.cpp
JWPLC_UI_Pages.cpp
JWPLC_UI_PixelMap.cpp
```

Por tanto el archive histórico no representa el source actual.

## Estrategia

H3E.1B.2 se divide en dos pasos.

### B2-A — generar candidate fuera del repo

El generador:

1. valida branch, dirty scope y hashes protegidos;
2. ejecuta el preflight del source setup ya validado;
3. produce nuevamente los objetos Display usando el mismo setup SOURCE de B1;
4. localiza `xtensa-esp32-elf-gcc-ar` desde el log real de compilación;
5. genera:

```txt
%TEMP%\jwplc_a14_h3e1b2_candidate_current\libJWPLC_Display.candidate.a
```

6. exige exactamente los 7 miembros esperados;
7. extrae cada miembro;
8. compara SHA de cada miembro contra su `.o` source;
9. escribe manifest JSON con HEAD, SHA del candidate, TUs y hashes;
10. confirma que el archive Alpha11, ModbusRTU y core.a permanecen intactos.

B2-A NO adopta ni reemplaza el archive versionado.

### B2-B — qualification física del candidate como archive

Pendiente después de cerrar B2-A.

El candidate se instalará temporalmente como:

```txt
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

sólo durante la compilación del Master.

Luego:

- se verificará 0 source TUs Display en el build candidate;
- se restaurará el archive histórico byte por byte;
- se subirá el binario ya enlazado con el candidate;
- se ejecutarán 300 s H3E.0B;
- se comparará contra Source B.

Objetivo de equivalencia:

```txt
candidate archive ~= source actual
```

No se exige byte-identidad de la aplicación completa, porque el linker puede
introducir padding/orden distinto entre source directo y archive. La validación
fuerte usa:

- miembros exactos;
- byte parity de cada miembro con su source object;
- prueba de linkage archive real;
- comportamiento físico;
- equivalencia de rendimiento.

## Candidate no final

El candidate H3E.1B.2 incluye el estado source actual, incluida la
instrumentación diagnóstica H3E.1 deshabilitada por default.

Por tanto:

```txt
CANDIDATE_FOR_DIAGNOSTIC=YES
FINAL_RELEASE_ARCHIVE=NO
```

El archive final de release se regenerará después de retirar/cerrar la
instrumentación diagnóstica correspondiente y repetir los gates de paridad.

## Invariantes protegidos

```txt
core.a
4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566

libJWPLC_ModbusRTU.a
444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F

libJWPLC_Display.a histórico
2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF
```

## Contrato preflight B2-A

Debe cerrar:

```txt
DISPLAY_SOURCE_TU_COUNT=7
H3E1B2_STATIC_PREFLIGHT=PASS
CANDIDATE_MUTATES_REPO_ARCHIVE=NO
PREFLIGHT_COMPILES=NO
PREFLIGHT_UPLOADS=NO
PREFLIGHT_GENERATES_CANDIDATE=NO
A14_H3E1B2_CANDIDATE_PREFLIGHT_ONLY=PASS
```

## Contrato generación B2-A

Debe cerrar:

```txt
SOURCE_OBJECT <cada TU>.o=1
CANDIDATE_MEMBER_COUNT=7
H3E1B2_CANDIDATE_MEMBERS_EXACT=PASS
H3E1B2_CANDIDATE_MEMBER_BYTE_PARITY=PASS
H3E1B2_HISTORICAL_ARCHIVE_PRESERVED=YES
A14_H3E1B2_CANDIDATE_GENERATION=PASS
```


## Incidencia B2-A — archiver no localizado desde log

La primera generación B2-A llegó correctamente hasta:

```txt
SOURCE_OBJECT JWPLC_Display.cpp.o=1
SOURCE_OBJECT JWPLC_Display_H3E1_Profile.cpp.o=1
SOURCE_OBJECT JWPLC_IdleScreen.cpp.o=1
SOURCE_OBJECT JWPLC_UI.cpp.o=1
SOURCE_OBJECT JWPLC_UI_API.cpp.o=1
SOURCE_OBJECT JWPLC_UI_Pages.cpp.o=1
SOURCE_OBJECT JWPLC_UI_PixelMap.cpp.o=1
```

y luego falló con:

```txt
H3E1B2_ARCHIVER_NOT_FOUND
```

Clasificación:

```txt
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
SOURCE_COMPILE=PASS
UPLOAD=PASS
SOURCE_OBJECT_SET=PASS
CANDIDATE_GENERATED=NO
HISTORICAL_ARCHIVE_PRESERVED=YES
```

Causa:

El generador nuevo intentaba obtener
`xtensa-esp32-elf-gcc-ar.exe` únicamente desde el compile log o como sibling
de una línea explícita de `g++`.

El log usado por el setup no garantizaba contener esas líneas, aunque la
toolchain sí estaba instalada y Arduino la había utilizado correctamente.

La fuente de verdad del package es:

```txt
platform.txt:
tools.xtensa-esp-elf-gcc.path={runtime.tools.esp-x32.path}
compiler.ar.cmd={compiler.prefix}gcc-ar

installed.json:
esp-x32@2601
```

Corrección:

1. mantener búsqueda en compile log como primera opción;
2. mantener sibling de compiler como segunda opción;
3. añadir fallback determinista al árbol real de Arduino15:
   `jwplc_local/tools/esp-x32/2601`;
4. mantener fallback secundario al namespace `jwplc`;
5. ejecutar `gcc-ar --version` ya en preflight;
6. no permitir una compilación física larga si el archiver no está resuelto;
7. permitir reusar un `TEMP_ROOT` source ya validado mediante
   `-ReuseSourceSetupRoot`, evitando recompilar/uploadar por un fallo posterior.

El build source válido de la incidencia fue:

```txt
C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h3e1b1_setup_20260927_191633
```

Regla preventiva:

```txt
No inferir herramientas Arduino únicamente desde verbose logs.

Para toolchains del package:
platform.txt / installed.json / Arduino15 tools tree
son fuentes más fuertes que la presencia opcional de una línea en el log.
```


## Resultado B2-A — candidate generado

La generación reutilizando el source build validado cerró:

```txt
SOURCE_OBJECT_ORIGIN=REUSE_EXISTING_SETUP
SOURCE_SETUP_ROOT=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h3e1b1_setup_20260927_191633

SOURCE_OBJECT JWPLC_Display.cpp.o=1
SOURCE_OBJECT JWPLC_Display_H3E1_Profile.cpp.o=1
SOURCE_OBJECT JWPLC_IdleScreen.cpp.o=1
SOURCE_OBJECT JWPLC_UI.cpp.o=1
SOURCE_OBJECT JWPLC_UI_API.cpp.o=1
SOURCE_OBJECT JWPLC_UI_Pages.cpp.o=1
SOURCE_OBJECT JWPLC_UI_PixelMap.cpp.o=1
```

Toolchain:

```txt
ARCHIVER_RESOLUTION=ARDUINO15_ESP_X32_2601
ARCHIVER_NAMESPACE=jwplc_local
ARCHIVER_TOOL_VERSION=2601
GNU ar 2.43.1
```

Candidate:

```txt
PATH=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h3e1b2_candidate_current\libJWPLC_Display.candidate.a
SHA256=B451A055E983B12B9BB2FBF134B8311B88E4CAB36B11EB89611CC36FAAF02BD0
BYTES=970776
MEMBERS=7
```

Miembros:

```txt
JWPLC_Display.cpp.o
JWPLC_Display_H3E1_Profile.cpp.o
JWPLC_IdleScreen.cpp.o
JWPLC_UI.cpp.o
JWPLC_UI_API.cpp.o
JWPLC_UI_Pages.cpp.o
JWPLC_UI_PixelMap.cpp.o
```

Validaciones:

```txt
H3E1B2_CANDIDATE_MEMBERS_EXACT=PASS
H3E1B2_CANDIDATE_MEMBER_BYTE_PARITY=PASS
H3E1B2_HISTORICAL_ARCHIVE_PRESERVED=YES
A14_H3E1B2_CANDIDATE_GENERATION=PASS
```

Estado:

```txt
B2_A_CANDIDATE_GENERATION=PASS
CANDIDATE_FOR_DIAGNOSTIC=YES
FINAL_RELEASE_ARCHIVE=NO
B2_B_PHYSICAL_QUALIFICATION=PENDING
```

## B2-B — qualification física del candidate como archive real

El leg H3E.1B.1 y su setup fueron parametrizados sin cambiar sus defaults para
aceptar:

```txt
ExpectedDisplayArchiveHash
AdditionalAllowedDirtyPaths
```

B2-B utiliza esos parámetros para:

1. verificar el candidate por SHA/tamaño/manifest;
2. verificar que el source Display no cambió desde el build del candidate;
3. respaldar el archive histórico Alpha11;
4. instalar temporalmente el candidate en la ruta precompiled real;
5. ejecutar el leg ARCHIVE ya validado;
6. exigir:
   ```txt
   DISPLAY_LINKAGE_PROOF=ARCHIVE_NO_SOURCE_OBJECTS
   MASTER_DISPLAY_SOURCE_OBJECT_COUNT=0
   ```
7. ejecutar la ventana H3E.0B de 300 s;
8. restaurar el archive Alpha11 en `finally`;
9. comparar contra el baseline source B1:
   ```txt
   SYS_DISPLAY AVG = 8575 us
   SYS_DISPLAY MAX = 9840 us
   RTU = 772.962 Hz
   TCP AVG = 1219.3 us
   TCP P99 = 9817.5 us
   SERVICE GAP MAX = 17315 us
   ```
10. declarar equivalencia de rendimiento si el AVG Display queda dentro de
    ±10 % del source y reportar por separado si el runtime fue completamente
    limpio.

El candidate permanece instalado únicamente alrededor del leg validado.
El archive histórico se restaura siempre mediante `finally`.

Marker preflight esperado:

```txt
H3E1B2B_STATIC_PREFLIGHT=PASS
PREFLIGHT_INSTALLS_CANDIDATE=NO
PREFLIGHT_COMPILES=NO
PREFLIGHT_UPLOADS=NO
PREFLIGHT_RUNS_300S=NO
A14_H3E1B2B_CANDIDATE_ARCHIVE_PREFLIGHT_ONLY=PASS
```

Markers finales de qualification:

```txt
H3E1B2_PERFORMANCE_EQUIVALENT=YES|NO
H3E1B2_RUNTIME_CLEAN=YES|NO
H3E1B2_RELEASE_EQUIVALENT=YES|NO
H3E1B2_CANDIDATE_INSTALLED_PERMANENTLY=NO
H3E1B2_DIAGNOSTIC_CAPTURE_VALID=YES
A14_H3E1B2B_CANDIDATE_ARCHIVE_QUALIFICATION_GATE=PASS
```


## Resultado final B2-B — qualification física

La qualification física posterior cerró el pendiente B2-B.

Comparación source actual vs archive candidate regenerado:

```text
H3E1B2_SOURCE_DISPLAY_AVG_US=8575
H3E1B2_CANDIDATE_DISPLAY_AVG_US=8204
H3E1B2_SOURCE_DISPLAY_MAX_US=9840
H3E1B2_CANDIDATE_DISPLAY_MAX_US=9522
H3E1B2_DISPLAY_AVG_DELTA_PCT=-4.327
H3E1B2_DISPLAY_MAX_DELTA_PCT=-3.232
```

Runtime:

```text
H3E1B2_SOURCE_RTU_HZ=772.962
H3E1B2_CANDIDATE_RTU_HZ=825.583

H3E1B2_SOURCE_TCP_AVG_US=1219.3
H3E1B2_CANDIDATE_TCP_AVG_US=1225.2

H3E1B2_SOURCE_TCP_P99_US=9817.5
H3E1B2_CANDIDATE_TCP_P99_US=9496.5

H3E1B2_SOURCE_SERVICE_GAP_MAX_US=17315
H3E1B2_CANDIDATE_SERVICE_GAP_MAX_US=11677
```

Clasificación:

```text
H3E1B2_AVG_EQUIVALENT=YES
H3E1B2_PERFORMANCE_EQUIVALENT=YES
H3E1B2_RUNTIME_CLEAN=YES
H3E1B2_RELEASE_EQUIVALENT=YES
H3E1B2_CANDIDATE_INSTALLED_PERMANENTLY=NO
H3E1B2_DIAGNOSTIC_CAPTURE_VALID=YES
A14_H3E1B2B_CANDIDATE_ARCHIVE_QUALIFICATION_GATE=PASS
```

El archive histórico fue restaurado después del leg:

```text
H3E1B2B_HISTORICAL_ARCHIVE_RESTORED=YES
```

Conclusión:

```text
B2_B_PHYSICAL_QUALIFICATION=PASS
PRECOMPILED_FULL_STRATEGY=VALIDATED
ROOT_CAUSE=HISTORICAL_DISPLAY_ARCHIVE_STALE
```

Este candidate B2 permaneció diagnóstico. El archive final de Display usado en
H3E.5 fue regenerado posteriormente, cualificado físicamente en H3E.3D y tiene:

```text
SHA256=52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986
```

Ver cierre consolidado:

- `A14_H3E_CLOSURE_20260928.md`
