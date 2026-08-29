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

## Pendientes de publicación

- [ ] Crear PR hacia `release/v2.1.x`.
- [ ] Revisar CI del PR.
- [ ] Hacer merge si los gates del PR son satisfactorios.
- [ ] Preparar ZIP de `v2.1.0-alpha.4`.
- [ ] Calcular tamaño y SHA-256 del ZIP.
- [ ] Actualizar el índice dev correspondiente.
- [ ] Crear tag de Alpha4.
- [ ] Crear GitHub PreRelease.
- [ ] Adjuntar ZIP.
- [ ] Validar instalación mediante Boards Manager.
- [ ] Validar compilación y subida desde la instalación publicada.

## Criterio de cierre técnico

La fase técnica de build-speed/cache de Alpha4 queda cerrada: rendimiento, app-only, bootloader, FlashFreq, particionado y gates físicos cuentan con una decisión explícita y evidencia registrada. Los puntos abiertos corresponden a integración/publicación de la alpha, no a nuevos experimentos de rendimiento.
