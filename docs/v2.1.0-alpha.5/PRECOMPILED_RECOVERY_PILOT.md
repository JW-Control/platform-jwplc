# Alpha5 - piloto de recuperación de precompilados

Este documento registra la recuperación incremental de archives compartidos mediante el bridge GPIO genérico validado en Alpha5.

## Estrategia

1. mantener la auditoría estricta como modo por defecto;
2. habilitar explícitamente `-AllowGenericGpioBridge` sólo para pilotos bridge-compatible;
3. permitir únicamente `jwplc_pinMode`, `jwplc_digitalWrite` y `jwplc_digitalRead` como dependencias externas bridge-compatible;
4. reactivar una librería por vez;
5. validar `ESP32 Board`, `JWPLC Basic` y `JWPLC Basic Core` antes de avanzar.

## Piloto 1 - JWPLC_Ethernet_W5x00_Backend

Motivo de prioridad: el backend representa 8 translation units recuperables, por lo que ofrece la mayor ganancia potencial individual.

Se reutiliza el mismo archive binario histórico de Alpha4 identificado por Git blob:

```txt
006fa25c31ba248d806feca986699471fa51c6ca
```

No se modifica el código fuente del backend ni su API pública.

### Gate estático bridge-compatible - PASS

Fecha: 2026-08-23.

Comando:

```powershell
pwsh -NoProfile -File ./tools/build-speed-benchmark/Audit-JWPLCPrecompiledLibraries.ps1 -AllowGenericGpioBridge
```

Resultado:

```txt
Modo bridge GPIO generico: HABILITADO
Archives encontrados: 8
[PASS] Adafruit_ST7735_and_ST7789_Library/libAdafruit_ST7735_and_ST7789_Library.a
[PASS] FS/libFS.a
[PASS] JW_FRAM/libJW_FRAM.a
[PASS] JW_RTC/libJW_RTC.a
[BRIDGE] JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a: jwplc_digitalWrite, jwplc_pinMode
[PASS] JWPLC_ModbusRTU/libJWPLC_ModbusRTU.a
[PASS] SPI/libSPI.a
[PASS] Wire/libWire.a
EXIT CODE: 0
```

Interpretación:

- los siete archives previamente neutrales siguen sin dependencias externas `jwplc_*`;
- el backend Ethernet requiere exclusivamente `jwplc_digitalWrite` y `jwplc_pinMode`, ambos cubiertos por el bridge GPIO genérico;
- no apareció ningún otro símbolo `jwplc_*` bloqueante;
- el gate estático del piloto 1 queda aprobado.

### Gate de enlace ESP32 Board - PASS

Fecha: 2026-08-23.

FQBN:

```txt
jwplc_local:esp32:esp32
```

Sketch:

```txt
tools/build-speed-benchmark/sketches/08_ethernet_bridge_link/08_ethernet_bridge_link.ino
```

Evidencia del log:

```txt
Skipping dependencies detection for precompiled library JWPLC Ethernet W5x00 Backend
Library JWPLC Ethernet W5x00 Backend has been declared precompiled:
Using precompiled library in ...\JWPLC_Ethernet_W5x00_Backend\src\esp32
...\cores\esp32\jwplc-gpio-compat.c
Sketch uses 271332 bytes (20%) of program storage space.
Global variables use 22172 bytes (6%) of dynamic memory, leaving 305508 bytes for local variables.
EXIT CODE: 0
```

Interpretación:

- Arduino Builder seleccionó el backend vendorizado del package JWPLC;
- el backend se consumió como archive precompilado y no se recompilaron sus 8 translation units;
- el core genérico compiló `jwplc-gpio-compat.c` en esta corrida, por lo que el gate no dependió de un `core.a` previo que ocultara el bridge;
- el enlace terminó sin referencias `jwplc_*` sin resolver.

### Gate de enlace JWPLC Basic - PASS

Fecha: 2026-08-23.

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

Sketch:

```txt
tools/build-speed-benchmark/sketches/01_empty/01_empty.ino
```

Evidencia:

```txt
Library JWPLC Ethernet W5x00 Backend has been declared precompiled:
Using precompiled library in ...\JWPLC_Ethernet_W5x00_Backend\src\esp32
Sketch uses 405553 bytes (9%) of program storage space. Maximum is 4063232 bytes.
Global variables use 27908 bytes (8%) of dynamic memory, leaving 299772 bytes for local variables.
```

No se detectaron referencias `jwplc_*` sin resolver ni error de build.

### Gate de enlace JWPLC Basic Core - PASS

Fecha: 2026-08-23.

FQBN:

```txt
jwplc_local:esp32:jwplcbasiccore
```

Sketch:

```txt
tools/build-speed-benchmark/sketches/01_empty/01_empty.ino
```

Evidencia:

```txt
Library JWPLC Ethernet W5x00 Backend has been declared precompiled:
Using precompiled library in ...\JWPLC_Ethernet_W5x00_Backend\src\esp32
Sketch uses 351396 bytes (11%) of program storage space. Maximum is 3145728 bytes.
Global variables use 27076 bytes (8%) of dynamic memory, leaving 300604 bytes for local variables.
```

No se detectaron referencias `jwplc_*` sin resolver ni error de build.

### Gate físico W5500 con RJ45 conectado - PASS

Fecha: 2026-08-23.

Sketch:

```txt
JWPLC/2.1.0/libraries/JWPLC_Ethernet/examples/Ethernet_Auto_DHCP_Status/Ethernet_Auto_DHCP_Status.ino
```

Resultado repetido durante la captura serial:

```txt
Enabled: yes | Attempted: yes | Ready: yes | HW: present | Link: UP | Status: OK | IP: 192.168.0.31
```

Interpretación:

- W5500 detectado físicamente;
- enlace Ethernet activo;
- DHCP funcional;
- runtime/autoload normal deja `Ready: yes` y `Status: OK`;
- la prueba de compilación/subida terminó con exit code 0.

### Gate físico sin RJ45 - PASS con observación

En una corrida sin cable Ethernet, el runtime reportó inicialmente durante algunos ciclos:

```txt
Enabled: yes | Attempted: yes | Ready: no | HW: present | Link: DOWN | Status: SPI lock timeout | IP: 0.0.0.0
```

Luego se estabilizó automáticamente en:

```txt
Enabled: yes | Attempted: yes | Ready: no | HW: present | Link: DOWN | Status: Link OFF | IP: 0.0.0.0
```

Interpretación:

- el hardware siguió siendo detectado;
- el estado final sin cable fue el esperado: enlace abajo e IP 0.0.0.0;
- el `SPI lock timeout` transitorio no bloqueó la recuperación del runtime;
- se registra como observación de estabilidad, no como fallo del piloto.

### Gate físico hot-plug - PASS

Fecha: 2026-08-23.

Condición:

1. arrancar el JWPLC Basic sin RJ45;
2. esperar a que el runtime estabilice `Link OFF`;
3. conectar el RJ45 sin reset ni reinicio;
4. observar recuperación automática y DHCP.

Secuencia observada en una misma captura serial:

```txt
Enabled: yes | Attempted: yes | Ready: no | HW: present | Link: DOWN | Status: Link OFF | IP: 0.0.0.0
...
Enabled: yes | Attempted: yes | Ready: no | HW: present | Link: UP | Status: Link OFF | IP: 0.0.0.0
...
Enabled: yes | Attempted: yes | Ready: yes | HW: present | Link: UP | Status: OK | IP: 192.168.0.31
```

Interpretación:

- el cambio de estado `DOWN -> UP` se detectó sin reiniciar el JWPLC Basic;
- el runtime recuperó automáticamente `Ready: yes`;
- DHCP obtuvo una IP válida;
- la conexión quedó estable en `Status: OK`;
- los `SPI lock timeout` observados al inicio desaparecieron sin intervención y no impidieron la recuperación.

## Estado del piloto 1 - CERRADO / ADOPTADO

Gates aprobados:

- auditoría bridge-compatible: PASS;
- ESP32 Board: PASS;
- JWPLC Basic: PASS;
- JWPLC Basic Core: PASS;
- W5500 con RJ45 conectado antes del arranque: PASS;
- W5500 sin RJ45: PASS con `SPI lock timeout` transitorio y recuperación a `Link OFF`;
- hot-plug RJ45 sin reset: PASS con transición automática a `Ready: yes`, `Status: OK` e IP DHCP válida.

Conclusión:

`JWPLC_Ethernet_W5x00_Backend` puede permanecer precompilado en Alpha5 usando el bridge GPIO genérico validado. El archive histórico reutilizado no rompe ESP32 Board, JWPLC Basic ni JWPLC Basic Core, y el W5500 conserva funcionamiento físico normal bajo autoload.

Observación abierta no bloqueante:

- conservar registrado el `SPI lock timeout` transitorio durante algunos arranques sin cable; actualmente el runtime se recupera automáticamente y no requiere reset.

## Piloto 2 - Adafruit_GFX_Library

Motivo de prioridad: recupera 4 translation units y es el siguiente bloque de mayor impacto después del backend Ethernet.

Se restaura el archive histórico de Alpha4 identificado por Git blob:

```txt
0b8b9ad2f7ce449a485635c702e08017194fa204
```

No se modifica el código fuente de Adafruit GFX ni su API pública.

### Gate estático bridge-compatible - PASS

Fecha: 2026-08-23.

Resultado de la auditoría global:

```txt
Modo bridge GPIO generico: HABILITADO
Archives encontrados: 9
[BRIDGE] Adafruit_GFX_Library/libAdafruit_GFX_Library.a: jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode
[PASS] Adafruit_ST7735_and_ST7789_Library/libAdafruit_ST7735_and_ST7789_Library.a
[PASS] FS/libFS.a
[PASS] JW_FRAM/libJW_FRAM.a
[PASS] JW_RTC/libJW_RTC.a
[BRIDGE] JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a: jwplc_digitalWrite, jwplc_pinMode
[PASS] JWPLC_ModbusRTU/libJWPLC_ModbusRTU.a
[PASS] SPI/libSPI.a
[PASS] Wire/libWire.a
EXIT CODE: 0
```

Interpretación:

- GFX requiere exclusivamente los tres símbolos GPIO permitidos por el bridge genérico;
- no apareció ningún `jwplc_*` adicional bloqueante;
- el backend Ethernet adoptado sigue bridge-compatible;
- los otros siete archives permanecen neutrales;
- resultado global: PASS BRIDGE-COMPATIBLE.

Estado del piloto 2: gate estático PASS; pendientes ESP32 Board, JWPLC Basic, JWPLC Basic Core y validación física del display antes de adoptar el archive.
