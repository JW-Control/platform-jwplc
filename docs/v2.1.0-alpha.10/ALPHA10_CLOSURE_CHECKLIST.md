# Alpha10 - Checklist de cierre

## Base y alcance

- [x] Rama creada desde `release/v2.1.x`.
- [x] Causa raíz reproducida.
- [x] Fallo primario identificado como `JWPLC_Ethernet` obsoleta en sketchbook.
- [x] Alcance reducido a hotfix de compatibilidad Arduino.
- [x] No se retiran periféricos del autoload.

## Corrección técnica

- [x] Marker exclusivo `JWPLC_Bundled_JWPLC_Ethernet.h`.
- [x] Marker cargado sólo durante `JWPLC_LIBRARY_DISCOVERY_PHASE`.
- [x] `JWPLC_Ethernet` actualizada a `1.0.1`.
- [x] API pública preservada.
- [x] Runtime sin cambios.
- [x] Archives precompilados sin cambios.

## Verificación de selección

- [x] Ruta de repo soportada.
- [x] `Arduino15/packages/jwplc` soportado.
- [x] `Arduino15/packages/jwplc_local` soportado.
- [x] Paths Windows escapados normalizados.
- [x] Sketchbooks no estándar detectados.
- [x] `JWPLC_Ethernet` del sketchbook detectada explícitamente por el verificador.
- [x] `JWPLC_ETHERNET_UNIFIED_SELECTION=PASS`.

## Regresión hostil

- [x] Se creó `JWPLC_Ethernet` hostil en sketchbook.
- [x] Compilación contaminada finaliza con exit code 0.
- [x] Copia package detectada.
- [x] Copia sketchbook no seleccionada.
- [x] Header hostil no ejecutado.
- [x] Undefined references = 0.
- [x] Cleanup de librería temporal completo.

## Matriz final

- [x] `DigitalIO_Basic` compila.
- [x] `Buttons_Basic` compila.
- [x] `Display_HMI_Fields` compila.
- [x] `Ethernet_Diagnostics` compila.
- [x] `RemoteIO_Slave_RTU` compila.
- [x] Matriz final = 5/5 PASS.
- [x] Undefined references = 0.

## Benchmark

- [x] Alpha9 exact base medido.
- [x] Alpha10 propuesta M7 medida.
- [x] Contraprueba en orden inverso realizada.
- [x] Paridad de compiladores confirmada.
- [x] Paridad de tamaño binario confirmada.
- [x] Perfil `M0/M1/M4/M7` realizado.
- [x] M7 descartado por `+37.4%` warm.
- [x] M4 descartado por `+21.7%` warm.
- [x] M1 adoptado con `+5.6%` warm.
- [x] Decisión de performance documentada.

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

## Commit técnico

- [x] Commit técnico creado.
- [x] Commit técnico subido al remoto.

```text
TECHNICAL_COMMIT_SHA=c0e5c621cec71977b86becfc8d7acb26ca21e906
```

## Documentación

- [x] Benchmark documentado.
- [x] Cierre técnico documentado.
- [x] Checklist actualizado.
- [x] Handoff Alpha10 -> Alpha11 documentado.
- [x] PR documentado.
- [x] PreRelease preparada.
- [ ] README actualizado a Alpha10.

## Publicación

- [ ] PR técnico abierto hacia `release/v2.1.x`.
- [ ] CI del PR en verde.
- [ ] PR integrado.
- [ ] PreRelease `v2.1.0-alpha.10` publicada.
- [ ] ZIP generado y checksum verificado.
- [ ] Índice dev actualizado.
- [ ] Índice estable sin cambios.
- [ ] PR de índices hacia `main` integrado.
- [ ] Cierre de publicación documentado.

## Pendientes transferidos a Alpha11

- [x] UI baudrate RTU del Backplane.
- [x] UI serial format RTU del Backplane.
- [x] Propagación de configuración RTU al HAL.
- [x] Referencias `TON0.Q` / `TOF0.Q` / `TP0.Q`.
- [x] Validación/autocomplete tipado de miembros FB.
- [x] Source freeze reproducible del fork OpenPLC Editor.
- [x] HMI Arduino hacia Ladder/OpenPLC.
- [x] Prueba multibit simultánea.
- [x] Estrategia de aislamiento general de librerías con menor coste de discovery.

## Estado actual

```text
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_DOCUMENTATION=IN_PROGRESS
ALPHA10_PUBLICATION=PENDING
```
