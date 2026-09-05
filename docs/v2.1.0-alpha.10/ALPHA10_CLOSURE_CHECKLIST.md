# Alpha10 - Checklist de cierre reabierto

## Base y alcance

- [x] Rama nueva creada desde `release/v2.1.x`.
- [x] Alpha10 previo identificado como release interno a reemplazar después de validar el candidato.
- [x] Alcance redefinido a limpieza de library discovery / recuperación de build speed.
- [x] Modelo soportado definido como `PACKAGE_MANAGED` para librerías JW/JWPLC.
- [x] No se retiran periféricos del autoload.

## Cambio técnico

- [x] Retirado `JWPLC_Bundled_JWPLC_Ethernet.h`.
- [x] `JWPLC_GlobalPeripherals_Auto.h` restaurado al comportamiento de Alpha9.
- [x] `JWPLC_Ethernet` restaurada a `1.0.0`.
- [x] Eliminado el verificador que exigía ignorar `JWPLC_Ethernet` del sketchbook.
- [x] API pública preservada.
- [x] Runtime preservado.
- [x] Archives precompilados preservados.

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Auditoría de protecciones

- [x] Guard JWPLC_Ethernet evaluado y retirado.
- [x] Markers Adafruit evaluados.
- [x] `JWPLC_Bundled_Adafruit_ST77xx.h` se conserva.
- [x] `JWPLC_Bundled_Adafruit_GFX.h` se conserva.
- [x] `JWPLC_Bundled_Adafruit_BusIO.h` se conserva.
- [x] `JWPLC_LIBRARY_DISCOVERY_PHASE` se conserva.
- [x] Motivo de cada decisión documentado.

## Benchmark

- [x] Evidencia histórica M0/M1/M4/M7 conservada.
- [ ] Candidato r1 ejecutado.
- [ ] Candidato r2 ejecutado.
- [ ] Candidato r3 ejecutado.
- [ ] Basic `01_empty` comparado.
- [ ] Core `01_empty` comparado.
- [ ] Basic/Core `02_io_basic` comparado.
- [ ] Cold evaluado.
- [ ] Warm no-change evaluado.
- [ ] Warm touch evaluado.
- [ ] Compiler invocations comparadas.
- [ ] Tamaño binario comparado.
- [ ] Conclusión final de build speed documentada.

## Matriz funcional

- [ ] `DigitalIO_Basic` compila.
- [ ] `Buttons_Basic` compila.
- [ ] `Display_HMI_Fields` compila.
- [ ] `Ethernet_Diagnostics` compila.
- [ ] `RemoteIO_Slave_RTU` compila.
- [ ] Matriz final = 5/5 PASS.
- [ ] Undefined references = 0.
- [ ] Arduino IDE compila con package local candidato.

## Validación física

- [ ] Boot y autoload normal.
- [ ] TFT/IDLE operativo.
- [ ] Botonera operativa.
- [ ] RTC visible/avanzando.
- [ ] Ethernet W5500 operativo.
- [ ] microSD/FRAM sin regresión observable en gate elegido.
- [ ] RS-485/Modbus RTU sin regresión observable en gate elegido.
- [ ] Sin congelamientos ni resets inesperados durante el smoke.

## Arquitectura y decisiones

- [x] Display permanece en autoload.
- [x] Ethernet permanece en autoload.
- [x] microSD permanece en autoload.
- [x] FRAM permanece en autoload.
- [x] RTC permanece en autoload.
- [x] Botonera permanece en autoload.
- [x] RS-485 permanece en autoload.
- [x] Modbus RTU permanece en autoload.
- [x] TCA/I/O permanece integrado.
- [x] OpenPLC no se declara integrado al runtime Arduino.
- [x] OTA no se asume definido.
- [x] `bootloader.bin` no se publica como definitivo.
- [x] Configuración Flash universal sigue pendiente.
- [x] App-only continúa como herramienta de desarrollo, no upload por defecto.
- [x] Bootloader precompilado continúa no adoptado.
- [ ] Conclusión de app-only revalidada en el cierre Alpha10.
- [ ] Conclusión de bootloader precompilado revalidada en el cierre Alpha10.
- [ ] Configuración final marcada como decidida o pendiente explícita.

## Documentación

- [x] Auditoría de protecciones creada.
- [x] Benchmark reabierto y procedimiento actualizado.
- [x] Cierre técnico reabierto y actualizado.
- [x] Checklist actualizado.
- [x] Handoff actualizado.
- [x] PR candidata redactada en español.
- [x] PreRelease candidata redactada en español.
- [ ] Tabla final de tiempos completada.
- [ ] README raíz actualizado después de los resultados finales.
- [ ] Documentos de transferencia del proyecto actualizados después del cierre.

## Publicación de reemplazo

- [ ] PR técnica lista para review después del benchmark.
- [ ] CI verde.
- [ ] Release/tag Alpha10 previo retirado sólo después de aprobar el candidato.
- [ ] PR integrada a `release/v2.1.x`.
- [ ] PreRelease `v2.1.0-alpha.10` regenerada.
- [ ] ZIP nuevo generado.
- [ ] SHA-256 nuevo registrado.
- [ ] Tamaño nuevo registrado.
- [ ] Índice dev actualizado al artefacto nuevo.
- [ ] Índice estable sin cambios.
- [ ] Instalación aislada desde índice publicado.
- [ ] Compilación aislada.
- [ ] Upload físico desde package publicado.
- [ ] Arranque post-upload.
- [ ] Cierre de publicación documentado.

## Estado actual

```text
ALPHA10_TECHNICAL_CHANGE=PASS
ALPHA10_DOCUMENTATION_DRAFT=PASS
ALPHA10_VALIDATION=PENDING_USER_GATE
ALPHA10_TECHNICAL_CLOSURE=PENDING
ALPHA10_PUBLICATION_REPLACEMENT=PENDING
NEXT_ALPHA=BLOCKED
```
