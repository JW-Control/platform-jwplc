# JWPLC v2.1.0-alpha.4 — checklist de cierre

## Identificación

- [x] Versión: `v2.1.0-alpha.4`.
- [x] Rama de trabajo: `v2.1.0-alpha.4/feature/build-speed-cache`.
- [x] Rama objetivo de integración: `release/v2.1.x`.
- [x] Objetivo: reducir tiempos de compilación manteniendo estabilidad, compatibilidad Arduino IDE y autoload normal de periféricos.

## Rendimiento de compilación

Fuente principal: `tools/build-speed-benchmark/JWPLC_ALPHA4_BUILD_SPEED_TIMINGS.md`.

- [x] Baseline Alpha3 instalado: **136.509 s** cold, 102 compilaciones/TUs.
- [x] Baseline local pre-D1: **148.649 s** cold.
- [x] D1 library discovery: **121.732 s**.
- [x] P1 librerías JWPLC precompiladas: **105.940 s**.
- [x] P2 core precompilado validado y compatible con caché de Arduino IDE.
- [x] P3 Display precompilado validado.
- [x] P4 GlobalPeripherals evaluado y rechazado por no mejorar el resultado.
- [x] P5 backend Ethernet W5x00 vendorizado/precompilado validado.
- [x] P6 stack Adafruit precompilado validado.
- [x] P7 FS + SD precompilados validados.
- [x] P8 Wire + SPI precompilados validados.
- [x] P8 final: **59.901 s** cold promedio en comparación A-B-B-A.
- [x] P8 reduce **4.985 s / 7.68 %** frente a Wire + SPI source-only en el mismo host.
- [x] Resultado final frente al baseline Alpha3 instalado: **76.608 s menos / ~56.12 %**.
- [x] TUs restantes con P8: **5**.
- [x] Validación cruzada en segundo equipo completada.
- [x] No se retiraron periféricos del autoload normal para obtener estas mejoras.

## App-only

Fuente: `tools/build-speed-benchmark/JWPLC_ALPHA4_APP_ONLY_CONCLUSION.md`.

- [x] App-only confirmado funcional como herramienta auxiliar de desarrollo.
- [x] No se adopta como upload normal por defecto.
- [x] El upload completo permanece como ruta normal/segura para Arduino IDE.
- [x] No se añade un menú público `UploadMode` en Alpha4.

## Bootloader y configuración de flash

Fuente: `tools/build-speed-benchmark/JWPLC_ALPHA4_BOOTLOADER_CONCLUSION.md`.

- [x] `FlashFreq=40 MHz` fijado para JWPLC Basic v2.0.
- [x] Flash mode: DIO.
- [x] Flash size: 4 MB.
- [x] `build.boot=qio`.
- [x] Dirección de bootloader: `0x1000`.
- [x] Bootloader reproducible generado desde `bootloader_qio_40m.elf`.
- [x] Bootloader de 40 MHz validado físicamente.
- [x] SHA-256 validado: `68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931`.
- [x] Bootloader antiguo de variante identificado como 80 MHz y retirado.
- [x] No se publica un `bootloader.bin` precompilado definitivo.

## Particionado de JWPLC Basic

Fuente: `tools/build-speed-benchmark/JWPLC_ALPHA4_PARTITION_CONCLUSION.md`.

- [x] Partición por defecto: `jwplc_max_app_4mb`.
- [x] APP máxima: **4,063,232 bytes / 3968 KiB / 3.875 MiB**.
- [x] Ganancia frente a `huge_app`: **917,504 bytes / 896 KiB / 29.17 %**.
- [x] NVS conservado.
- [x] `otadata` conservado.
- [x] Coredump de 64 KiB conservado.
- [x] SPIFFS retirado del perfil JWPLC Basic.
- [x] PRT1–PRT5 completados.
- [x] Default real del board compilado, subido y validado físicamente.
- [x] `JWPLC Basic Core` permanece con `huge_app` y se evaluará de forma independiente.

## Autoload y compatibilidad

- [x] Display conservado.
- [x] Ethernet conservado.
- [x] microSD conservada.
- [x] FRAM conservada.
- [x] RTC conservado.
- [x] Botonera conservada.
- [x] RS-485 conservado.
- [x] Modbus RTU conservado.
- [x] TCA / I/O conservado.
- [x] Mutex SPI global conservado.
- [x] Arduino CLI validado.
- [x] Arduino IDE validado.
- [x] JWPLC Basic compila.
- [x] JWPLC Basic Core compila en gates de compatibilidad.
- [x] ESP32 genérico compila en gates de compatibilidad P8.

## Gates físicos

Fuentes:

- `tools/build-speed-benchmark/JWPLC_ALPHA4_LOCAL_PHYSICAL_GATE.md`
- `tools/build-speed-benchmark/JWPLC_ALPHA4_COMMUNICATION_PHYSICAL_GATES.md`

- [x] Display ready.
- [x] RTC.
- [x] FRAM.
- [x] microSD.
- [x] Seis botones físicos UP/DOWN/LEFT/RIGHT/CANCEL/OK.
- [x] 8 entradas digitales.
- [x] 8 salidas/relés.
- [x] TFT visual.
- [x] W5500 / DHCP.
- [x] Ethernet + TFT + FRAM + SD coexistencia.
- [x] HTTP físico.
- [x] RS-485 físico.
- [x] Modbus RTU FC03.
- [x] Modbus RTU FC06.
- [x] Gate físico final PRT5C: `ALPHA4_LOCAL_PHYSICAL_GATE=PASS`.
- [x] Arranque ROM observado: `mode:DIO, clock div:2`.

## Decisiones de alcance

No forman parte del cierre de Alpha4:

- [x] No asumir OpenPLC integrado como parte de esta optimización.
- [x] No definir OTA en esta alpha.
- [x] No migrar a FlashFreq 80 MHz en esta alpha.
- [x] No migrar JWPLC Basic Core a Max App sin gates propios.
- [x] No mezclar la futura arquitectura ESP32-S3 con la decisión actual de JWPLC Basic v2.0.
- [x] No iniciar P9 antes de cerrar Alpha4.

## Documentación pública de cierre

- [x] Tabla formal de tiempos.
- [x] Conclusión app-only.
- [x] Conclusión bootloader.
- [x] Conclusión de particionado.
- [x] Gates físicos documentados.
- [x] `PULL_REQUEST.md` preparado en español.
- [x] `PRE_RELEASE.md` preparado en español.
- [x] Validación de publicación standalone documentada en `ALPHA4_PUBLICATION_VALIDATION.md`.
- [x] README general actualizado al estado de Alpha4.

## Publicación

- [x] PR técnico #62 hacia `release/v2.1.x` creado y fusionado.
- [x] CI de publicación ejecutado satisfactoriamente.
- [x] ZIP `jwplc-esp32-2.1.0-alpha.4.zip` generado.
- [x] Tamaño final: **24,698,966 bytes**.
- [x] SHA-256 final: `4bdbdf383bb863d8f1a3b22f2b88ce47c68d2044a22f0f4afb6d883ccfdade5b`.
- [x] Tag `v2.1.0-alpha.4` creado y congelado sobre `444282f560065fb9ae4cb0ba7d4067877410ca0f`.
- [x] GitHub PreRelease creada.
- [x] ZIP adjuntado a la PreRelease.
- [x] Índice dev actualizado con Alpha4 mediante PR #63.
- [x] Índice público estable conservado en `v2.0.0`.
- [x] Integración final de Alpha4 a `main` mediante PR #64.

## Validación standalone posterior a publicación

Fuente: `docs/v2.1.0-alpha.4/ALPHA4_PUBLICATION_VALIDATION.md`.

- [x] Entorno Arduino CLI aislado sin plataformas JWPLC previamente instaladas.
- [x] Índice dev descargado desde `main`.
- [x] `jwplc:esp32@2.1.0-alpha.4` visible antes de instalar.
- [x] Instalación standalone completada.
- [x] Las cinco `toolsDependencies` fueron descargadas automáticamente.
- [x] `01_empty` compiló desde el package instalado: **44.102 s**.
- [x] `03_autoload_contract` compiló desde el package instalado: **42.460 s**.
- [x] Upload standalone por USB a JWPLC Basic físico completado.
- [x] Gate físico local recompilado usando el package publicado.
- [x] Máximo de aplicación observado: **4,063,232 bytes**.
- [x] Display, RTC, FRAM, SD, botonera, 8 DI, 8 DO y TFT visual: PASS.
- [x] Resultado: `ALPHA4_STANDALONE_INSTALL=PASS`.
- [x] Resultado: `ALPHA4_STANDALONE_COMPILE_GATE=PASS`.
- [x] Resultado: `ALPHA4_STANDALONE_UPLOAD=PASS`.
- [x] Resultado: `ALPHA4_LOCAL_PHYSICAL_GATE=PASS`.

## Criterio de cierre final

Alpha4 queda cerrada técnica y públicamente.

Se dispone de evidencia para:

- rendimiento de compilación;
- app-only;
- bootloader y FlashFreq;
- particionado Max App;
- gates físicos;
- publicación del ZIP;
- índice dev;
- instalación limpia;
- compilación standalone;
- subida standalone a hardware real;
- funcionamiento físico posterior a publicación.

No quedan pendientes de Alpha4 que requieran nuevos experimentos técnicos. Cualquier cambio adicional debe tratarse como documentación de mantenimiento, tooling o trabajo de una alpha posterior.
