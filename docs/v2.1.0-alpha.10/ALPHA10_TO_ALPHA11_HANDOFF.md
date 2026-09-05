# Transferencia Alpha10 -> Alpha11

## Regla de avance

Alpha11 no debe iniciar como trabajo de release hasta que Alpha10 quede nuevamente cerrado y publicado con el benchmark actualizado.

```text
NEXT_ALPHA=BLOCKED_UNTIL_ALPHA10_CLOSED
```

## Qué cambia Alpha10

Alpha10 queda acotado a limpieza de build/library discovery:

```text
JWPLC_ETHERNET_SHADOW_GUARD=REMOVED
JWPLC_ETHERNET_VERSION=1.0.0
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
ADAFRUIT_BUNDLED_MARKERS=RETAINED
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Commit técnico:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Qué debe cerrar Alpha10 antes de transferir

- benchmark r1/r2/r3;
- tabla final de tiempos;
- matriz funcional 5/5;
- smoke físico;
- Arduino IDE + Arduino CLI;
- conclusión app-only;
- conclusión bootloader precompilado;
- configuración final decidida o pendiente explícita;
- PR/PreRelease/checklist finales;
- reemplazo del artefacto Alpha10 previo;
- instalación y upload desde el índice publicado.

## Decisiones que Alpha11 hereda

```text
PACKAGE_MANAGED_JW_LIBRARIES=YES
MANUAL_JW_LIBRARY_SHADOWING_GUARD=NO
THIRD_PARTY_ADAFRUIT_SELECTION_GUARDS=YES
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
FLASH_CONFIGURATION_FINAL=PENDING_UNLESS_CLOSED_IN_ALPHA10
BOOTLOADER_BIN_FINAL=NO
```

Alpha11 no debe reintroducir markers generalizados de librerías JW/JWPLC sin un benchmark que demuestre que el coste y la necesidad están justificados.

## Alcance funcional de Alpha11

Este documento no redefine el alcance funcional de Alpha11. Cualquier trabajo posterior debe partir del cierre real de Alpha10 y de la planificación vigente del proyecto en ese momento.

## Estado

```text
ALPHA10_TO_ALPHA11_HANDOFF=PREPARED
HANDOFF_EXECUTION=PENDING_ALPHA10_CLOSURE
```
