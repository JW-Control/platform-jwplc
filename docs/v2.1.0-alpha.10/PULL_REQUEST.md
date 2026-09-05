# PR - Alpha10: retirar guard de shadowing y recuperar build speed

## Resumen

Esta PR reabre `v2.1.0-alpha.10` para retirar el guard de library discovery añadido a `JWPLC_Ethernet` y volver a medir el ciclo normal de compilación.

El guard resolvía una copia antigua/manual de `JWPLC_Ethernet` instalada en el sketchbook, pero el benchmark mostró un coste warm aproximado de `+1.233 s / +5.6%` incluso con un único marker.

Se adopta como contrato que las librerías propias `JW_*` / `JWPLC_*` son administradas por el package. Las instalaciones manuales paralelas quedan fuera del modelo soportado.

## Cambios

- elimina `JWPLC_Bundled_JWPLC_Ethernet.h`;
- restaura `JWPLC_GlobalPeripherals_Auto.h` al comportamiento de Alpha9;
- restaura metadata de `JWPLC_Ethernet` a `1.0.0`;
- retira el verificador específico que exigía ignorar una `JWPLC_Ethernet` del sketchbook;
- conserva el smoke CI sobre periféricos soportados;
- conserva los markers Adafruit heredados;
- reabre la documentación y benchmark de Alpha10.

Commit técnico:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Por qué se conservan los markers Adafruit

Los markers de ST77xx/GFX/BusIO no se consideran equivalentes al guard de `JWPLC_Ethernet`.

Adafruit es una dependencia externa y es normal que existan otras versiones instaladas mediante Library Manager. Además, el stack vendorizado utiliza archives precompilados. En Alpha5 se registró una selección real de BusIO desde el sketchbook durante un gate genérico, por lo que la selección reproducible sigue siendo una necesidad de compatibilidad.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
```

Se mantienen integrados Display, Ethernet, microSD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O.

## Benchmark histórico

| Configuración | Warm promedio | Delta |
|---|---:|---:|
| 0 markers JW/JWPLC | 22.094 s | base |
| Ethernet únicamente | 23.327 s | +5.6% |
| 4 markers | 26.888 s | +21.7% |
| 7 markers | 30.353 s | +37.4% |

## Gates pendientes antes de merge

- [ ] benchmark candidato r1;
- [ ] benchmark candidato r2;
- [ ] benchmark candidato r3;
- [ ] comparación Basic/Core;
- [ ] `01_empty` y `02_io_basic`;
- [ ] matriz funcional 5/5;
- [ ] Arduino IDE;
- [ ] smoke físico;
- [ ] CI verde;
- [ ] tabla final de tiempos y conclusiones actualizadas.

## Publicación

El release/tag Alpha10 previo no se elimina como parte de este PR antes de validar el candidato. Después de aprobar los gates se retirará/reemplazará y se generará un nuevo ZIP/SHA para `v2.1.0-alpha.10`.

```text
ALPHA10_RELEASE_REPLACEMENT=PENDING_VALIDATION
```
