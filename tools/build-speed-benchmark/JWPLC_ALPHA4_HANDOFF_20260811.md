# JWPLC Alpha4 - Handoff de continuidad

Fecha de corte: 2026-08-11 01:28 (UTC-5)

Repositorio: `JW-Control/platform-jwplc`

Rama activa: `v2.1.0-alpha.4/feature/build-speed-cache`

Commit base validado antes de este handoff:

`3e4abbbd69ff283d762fee6dce4ed095fb755ae3`

Asunto:

`fix(ethernet): evitar retencion SPI de 560 ms al arrancar W5500`

Este documento sirve para iniciar un chat nuevo sin reconstruir el contexto desde cero.

---

## 1. Reglas de trabajo que deben mantenerse

Prioridades del package JWPLC:

1. Estabilidad.
2. Compatibilidad con Arduino IDE.
3. No romper APIs ya probadas.
4. Registrar decisiones.
5. Cerrar pendientes antes de avanzar a otra etapa.
6. Documentación técnica en español.

Reglas de arquitectura vigentes:

- No retirar periféricos del autoload normal sólo por velocidad de compilación.
- No asumir OpenPLC integrado.
- No asumir OTA definido.
- No asumir FlashFreq final.
- No publicar `bootloader.bin` como definitivo hasta fijar la configuración final.
- Si una corrección real exige tocar el core, se puede hacer, siempre que se preserve compatibilidad y se repitan los gates correspondientes.
- El mutex SPI debe mantenerse. La solución nunca debe permitir accesos simultáneos de TFT, W5500, SD y FRAM.

Antes de cerrar Alpha4 deben mantenerse explícitas las conclusiones sobre:

- tiempos de compilación;
- app-only;
- bootloader precompilado;
- configuración final o pendiente explícito;
- validación física;
- PR y PreRelease en español;
- checklist actualizado.

---

## 2. Forma de trabajo con PowerShell

Preferencia del usuario:

- Entregar un solo bloque de PowerShell por paso.
- Esperar la salida antes de continuar.
- No encadenar varios experimentos distintos en un mismo mensaje salvo que sea una secuencia inseparable, por ejemplo upload + monitor.
- No usar `git add .`.
- Stagear rutas explícitas.
- No hacer commit antes de revisar gates y `git status`.

Rutas de esta laptop:

Repositorio:

`C:\Users\jeykc\Documents\GitHub\platform-jwplc`

Arduino CLI:

`C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe`

Git de GitHub Desktop:

`$env:LOCALAPPDATA\GitHubDesktop\app-3.6.3\resources\app\git\cmd\git.exe`

FQBN local:

`jwplc_local:esp32:jwplcbasic`

Puerto físico usado en esta sesión:

`COM3`

Baud de monitor:

`115200`

### Lecciones prácticas de PowerShell/Git de esta sesión

1. En esta instalación Git puede intentar usar `less` y fallar con:

   `cannot spawn less: No such file or directory`

   Usar siempre una de estas opciones:

   - `git --no-pager ...`
   - o definir `GIT_PAGER=cat` y `PAGER=cat`.

2. Para validar archivos untracked individualmente, no usar sólo `git status --short`, porque Git puede resumir un directorio completo como:

   `?? tools/.../06_alpha4_local_physical_gate/`

   Usar:

   `git status --porcelain=v1 --untracked-files=all`

3. Para parches de texto en Windows, no depender de regex que asuman LF. El archivo puede usar CRLF y el patrón puede fallar aunque el texto exista.

4. Para cambios únicos y conocidos, preferir:

   - contar apariciones literales;
   - exigir exactamente una coincidencia;
   - reemplazar sólo esa cadena;
   - verificar antes de escribir.

5. No usar reemplazos que dependan de un bloque completo con espacios, tabs o saltos de línea exactos si no es necesario.

6. Para documentos Markdown grandes, preferir creación directa mediante GitHub/archivo en vez de pegar Markdown complejo dentro de un bloque PowerShell del chat. En esta sesión el formato visual se degradó varias veces al copiar here-strings largos.

7. Antes de commit:

   - validar exactamente los archivos esperados;
   - `git diff --cached --check` con `--no-pager`;
   - `git diff --cached --stat`;
   - verificar que no existan archivos staged extra.

---

## 3. Estado de optimización P1-P8

### Baseline histórico

Alpha3 oficial cold administrado:

- 136.509 s
- 102 compilaciones reales

Local pre-D1:

- cold 148.649 s
- warm 36.523 s
- touch 40.524 s

### D1 - Library discovery

Se añadió una fase ligera de descubrimiento para evitar trabajo innecesario en compilaciones incrementales.

Commit relevante:

`a78b14e`

Resultado aproximado:

- cold 121.732 s
- warm/touch alrededor de 14 s

### P1 - Librerías JWPLC precompiladas

Precompiladas:

- JW_RTC
- JW_FRAM
- JW_SD
- JW_MatrixButtons
- JWPLC_ModbusRTU

Con `precompiled=full` y archive `src/esp32/lib*.a`.

Resultado controlado de referencia:

- source 123.362 s
- P1 105.940 s
- TUs 102 -> 97

### P2 - Core precompilado

El JWPLC Basic usa core precompilado separado:

`JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a`

Integración:

- `jwplcbasic.build.core=jwcontrol_p2`
- `jwplcbasic.build.extra_libs=...core.a`

El Basic Core NO reutiliza este archive.

Commit relevante:

`7673ca1`

Posteriormente se aisló correctamente el include del core P2 por placa.

Commit:

`d971f81`

### P3 / P4

P3 Display fue útil.

P4 GlobalPeripherals fue rechazado porque aumentaba acoplamiento y no mejoraba de forma suficiente.

No volver a precompilar GlobalPeripherals sin una razón nueva y evidencia.

### P5A - Ethernet backend

Se vendorizó Arduino Ethernet 2.0.2 como:

`JWPLC_Ethernet_W5x00_Backend`

Commit inicial relevante:

`0916663`

Resultado P5A histórico:

- 90.587 s
- 24 compiles

### P6 - Adafruit

Precompiladas:

- Adafruit ST77xx
- Adafruit GFX
- Adafruit BusIO

Resultado controlado principal:

- 67.322 s
- 12 TUs

Commit P1-P6:

`fc29e26`

### P7 - FS + SD

Cerrado y adoptado.

Archives:

FS:

- 415104 B
- SHA-256 `CBEA33C505D28E9B3A6A2E3ABDDEA6CB4B8BE919ED456C00ED0D8A31C327`

SD:

- 275694 B
- SHA-256 `45D1D9B27701403CE7D380838AD723194A3730DB5F2859B90D0D01D75FA040FD`

Laptop P7:

- discovery 50.004 s
- 7 TUs
- secuencial 13.351 s
- cold j0 63.870 s

Gate físico SD pasó después de corregir una falla física de soldadura en GPIO39 card-detect.

Commits relevantes:

- `1a60176`
- `f593396`
- `7d328245ae9723b294026e5863e858ad7d7971f9`

### P8 - Wire + SPI

Cerrado y adoptado.

Se encontró y corrigió un bug preexistente de compatibilidad Wire/JWPLC:

`Wire.begin()` podía devolver true si I2C ya estaba inicializado, pero sin haber reservado buffers.

La corrección movió `allocateWireBuffer()` antes del early return por `i2cIsInit(num)`.

Wire archive final:

- 166980 B
- SHA-256 `A864851EBFCB8CD3FEE55D3D7834B81254AD4EBE0D75F6ED9EBC846355F9C4AA`

SPI archive:

- 79714 B
- SHA-256 `F9883DFD39CA299F7CB76673744F8F0DE4EA01C5A53F186A3841199FCA289245`

Gate físico combinado:

- `P8_WIRE_GATE=PASS`
- `P8_SPI_GATE=PASS`
- `P8_WIRE_SPI_GATE=PASS`

A-B-B-A laptop con cargador:

- source promedio 64.885 s
- P8 promedio 59.901 s
- mejora 4.985 s / 7.68 %

Commit P8:

`82fe109`

No iniciar P9 por ahora. La prioridad pasó a cierre físico y de release.

---

## 4. App-only

Cerrado.

Documento:

`JWPLC_ALPHA4_APP_ONLY_CONCLUSION.md`

Commit:

`26bc631`

Conclusión:

- app-only puede ser útil para desarrollo;
- no se adopta como flujo de upload por defecto;
- full upload sigue siendo el camino seguro por defecto.

---

## 5. Bootloader precompilado

Cerrado.

Documento:

`JWPLC_ALPHA4_BOOTLOADER_CONCLUSION.md`

Commit:

`41e3476`

Resultado histórico:

- sin bootloader: cold 121.014 s / incremental 31.861 s
- bootloader precompilado: cold 118.141 s / incremental 34.321 s

Conclusión:

- beneficio inconsistente;
- no adoptado;
- no publicar `bootloader.bin` definitivo todavía.

Configuración probada para Alpha4 Basic en ese experimento:

- 240 MHz
- 4 MB
- 40 MHz
- DIO
- QIO
- huge_app
- upload 921600

No asumirla como configuración final universal del producto.

---

## 6. Contrato final de autoload

Cerrado a nivel compile/link.

Sketch:

`tools/build-speed-benchmark/sketches/03_autoload_contract/03_autoload_contract.ino`

Resultado:

`ALPHA4_AUTOLOAD_CONTRACT=PASS`

Referencia de compilación:

- 53.582 s, sólo referencia
- app 394709 B
- globals 27612 B

El log confirmó uso de P2 y archives P1-P8.

Commit:

`cdd2d32`

Este gate sólo cubre compile/link, no hardware físico.

---

## 7. Gate físico local Alpha4

### Sketch final

`tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino`

El gate cubre:

- Display ready;
- RTC;
- FRAM;
- SD;
- 6 botones físicos;
- 8 entradas digitales;
- 8 salidas por relé;
- confirmación visual TFT.

Ethernet y RS-485/Modbus quedan fuera de este gate y se validan aparte.

### Botonera correcta

El primer sketch tenía por error IDs de otro producto.

JWPLC Basic real:

- UP
- DOWN
- LEFT
- RIGHT
- CANCEL
- OK

CANCEL corresponde internamente a `BTN_ESC`.

No volver a usar INFO / CONFIG / START para el JWPLC Basic.

### Primer resultado del gate

Pasaron:

- Display
- RTC
- botones
- 8 DI
- 8 DO
- TFT visual

Fallaron:

- FRAM
- SD

Firma inicial:

- FRAM `size()` = 8192
- dos lecturas FRAM = FAIL
- SD = `SPI lock timeout`

Esto inició la investigación de contención SPI.

---

## 8. Diagnóstico de contención SPI

Bus SPI compartido:

- TFT
- W5500
- microSD
- FRAM

Pines:

- MOSI 23
- MISO 19
- SCK 18
- TFT CS 33
- SD CS 32
- FRAM CS 13
- ETH CS 5

Frecuencias definidas actualmente:

- TFT 80 MHz
- Ethernet 20 MHz
- SD 20 MHz
- FRAM 10 MHz

Mutex general:

- `jwplcSPI_acquire(timeoutMs)`
- `jwplcSPI_release()`

El mutex utiliza `xSemaphoreCreateMutex()` normal.

No se detectó una ruta evidente de `acquire` sin `release` en Display ni Ethernet.

### Probe antes del Display

FRAM y SD pasaron 30/30 antes de que Display terminara su autoload.

Eso descartó una avería física simultánea.

### Error de diseño del primer probe post-Display

Se intentó esperar `JWPLC_Display.isReady()` dentro de `setup()`.

Eso produce deadlock lógico de prueba porque `jwplcSystemTask` arranca después de que `setup()` retorna.

Lección:

- no esperar dentro de `setup()` condiciones que dependen de `jwplcSystemTask`;
- hacer ese tipo de espera desde `loop()`.

### Probe post-Display correcto

Fase A inmediatamente después de Display ready:

- FRAM OK 26 / FAIL 4
- SD OK 27 / FAIL 3
- FRAM max ~51 ms
- SD max ~100 ms

Fase B operación normal:

- FRAM 30/30 PASS
- SD 30/30 PASS

Fase C redraws forzados:

- FRAM 20/20 PASS
- SD 20/20 PASS
- FRAM max ~7.6 ms

Conclusión:

- no había lock permanente;
- no había incompatibilidad normal TFT/FRAM/SD;
- la contención problemática era exclusiva del arranque.

---

## 9. Causa raíz W5500 identificada

El probe de propietario mostró antes del fix:

- Ethernet `attempted=SI`, `ready=NO`, `error=OK` mientras FRAM y SD agotaban sus timeouts;
- alrededor de 589 ms Ethernet terminaba en `Link OFF`;
- inmediatamente después FRAM y SD volvían a funcionar.

Se revisó el backend vendorizado y se encontró en `W5100Class::init()`:

`delay(560);`

Ese delay proviene de Arduino Ethernet 2.0.2 para shields genéricos que pueden usar CAT811/MAX811.

En JWPLC Basic es redundante porque `JWPLC_Ethernet` ya realiza reset explícito del W5500:

- RESET LOW 10 ms
- RESET HIGH
- espera 80 ms
- luego adquiere el mutex SPI

El problema era que el backend ejecutaba el `delay(560)` después de que Ethernet ya había adquirido el mutex global SPI.

Durante esos ~560 ms:

- no se hacía una transferencia SPI útil;
- FRAM agotaba su timeout de ~50 ms;
- SD agotaba su timeout de ~100 ms.

### Corrección adoptada

Se eliminó únicamente el `delay(560)` del backend JWPLC.

No se modificaron:

- APIs públicas;
- mutex SPI;
- timeouts FRAM/SD;
- autoload;
- arquitectura general del core.

No se quitó ningún periférico.

El backend debe considerarse desde ahora:

`Arduino Ethernet 2.0.2 + patch específico JWPLC`

No es byte-identical respecto al upstream.

### Archive Ethernet regenerado

Antes:

- 1,116,812 B
- SHA-256 `C1637EBBE782C24AA8449CCFDD4D04EA3E4B416ED3DDB4C8A885890C24C273E3`

Después:

- 1,116,456 B
- SHA-256 `F3D4A7922F6ECB916F91B51D13E0AF02DA194529CB1A36D52332C938EACC2097`

El archive fue regenerado con `Build-JWPLCPrecompiledLibraries.ps1` seleccionando sólo `JWPLC_Ethernet_W5x00_Backend`.

### Verificación posterior al fix

Muestra 1 después del fix:

- Ethernet ya en `Link OFF`
- FRAM = OK, ~27.9 ms
- SD = OK, ~0.63 ms

Luego FRAM vuelve a ~80-90 us y SD ~380-420 us.

Interpretación:

- la contención real y legítima sigue existiendo;
- el mutex sigue funcionando;
- FRAM puede esperar por el bus;
- ya no existe una retención artificial de 560 ms.

---

## 10. Gate físico local final

Después del fix W5500 y de recompilar el gate integral:

- `ALPHA4_DISPLAY_READY=PASS`
- `ALPHA4_RTC=PASS`
- `ALPHA4_FRAM=PASS`
- `ALPHA4_SD=PASS`
- `ALPHA4_BUTTONS=PASS`
- `ALPHA4_INPUTS=PASS`
- `ALPHA4_OUTPUTS=PASS`
- `ALPHA4_DISPLAY_VISUAL=PASS`
- `ALPHA4_LOCAL_PHYSICAL_GATE=PASS`

FRAM final:

- size 8192
- read #1 OK
- read #2 OK
- lecturas iguales

SD final:

- readback `JWPLC_ALPHA4_SD_OK`
- remove OK

Entradas:

- I0_0 a I0_7 activadas y regresadas a estado inicial

Salidas:

- Q0_0 a Q0_7 verificadas físicamente una por una

Botones:

- UP
- DOWN
- LEFT
- RIGHT
- CANCEL
- OK

TFT:

- confirmación visual PASS
- el gate forzó PWR/RUN/BUS/ETH verde y ERR rojo deliberadamente
- ERR rojo en ese gate NO indica una falla automática

Documento detallado:

`tools/build-speed-benchmark/JWPLC_ALPHA4_LOCAL_PHYSICAL_GATE.md`

Commit y push de cierre:

`3e4abbbd69ff283d762fee6dce4ed095fb755ae3`

Push confirmado a:

`origin/v2.1.0-alpha.4/feature/build-speed-cache`

Working tree quedó limpio después del push.

---

## 11. Siguiente tarea recomendada

No iniciar P9.

El siguiente paso lógico es el gate físico funcional de Ethernet W5500.

Debe validar como mínimo:

1. Detección real del W5500.
2. Estado de link con cable conectado.
3. Obtención de IP por DHCP o configuración estática de prueba.
4. Comunicación real, idealmente ping y/o HTTP.
5. Convivencia SPI mientras Ethernet está activo:
   - TFT operativo;
   - SD accesible;
   - FRAM accesible.
6. Confirmar que el patch que eliminó `delay(560)` no afecta la inicialización funcional del W5500.

Antes de escribir un nuevo sketch, revisar los ejemplos existentes del repo, especialmente los relacionados con:

- Ethernet Auto DHCP Status;
- Ethernet SPI Coexistence;
- ejemplos de HTTP si existen.

No adivinar APIs: inspeccionar los ejemplos y headers reales de la rama.

Después del gate Ethernet:

- gate físico RS-485;
- gate Modbus RTU;
- consolidar gate físico global Alpha4.

---

## 12. Pendientes posteriores de Alpha4

Después de cerrar Ethernet y RS-485/Modbus, todavía revisar:

- decisión o pendiente explícito de configuración final, incluida FlashFreq;
- no asumir bootloader final;
- verificar Basic Core y su estrategia P2 sin reutilizar el archive de Basic;
- revisar equivalencia/compatibilidad de archives cuando corresponda;
- auditoría `#ifdef JWPLC_HAS_*` versus `#if JWPLC_HAS_*`;
- corregir o retirar gates antiguos que ya no representen el flujo real;
- prueba con package administrado y/o entorno limpio/fresh clone;
- tabla final de tiempos;
- checklist de release;
- PR en español;
- PreRelease en español.

---

## 13. Comandos y prácticas que NO repetir

- No esperar `JWPLC_Display.isReady()` dentro de `setup()` si depende del task de sistema.
- No asumir que dos fallos SPI simultáneos son dos fallas físicas independientes.
- No aumentar timeouts para ocultar una retención artificial del bus.
- No retirar Ethernet/Display/SD/FRAM del autoload para evitar contención.
- No cambiar el mutex por una solución sin exclusión.
- No regenerar todos los archives si sólo cambió una librería y el script permite `-Libraries`.
- No usar `git add .`.
- No usar validaciones de `git status` que resuman directorios untracked cuando se necesita validar archivos exactos.
- No hacer parches basados en bloques de texto excesivamente exactos cuando basta una coincidencia literal única.
- No asumir que un archivo fuente modificado se usa en runtime si la biblioteca está en `precompiled=full`: regenerar siempre su `.a` antes de probar.

---

## 14. Frase de arranque sugerida para un chat nuevo

Copiar al iniciar el siguiente chat:

> Continúa Alpha4 del package JWPLC desde `tools/build-speed-benchmark/JWPLC_ALPHA4_HANDOFF_20260811.md`. Corrobora primero la rama y el HEAD remoto. El gate físico local ya cerró en PASS y el siguiente objetivo es el gate Ethernet funcional W5500 sin retirar periféricos del autoload. Trabaja un bloque PowerShell a la vez y no asumas APIs: inspecciona primero los ejemplos y headers reales del repo.
