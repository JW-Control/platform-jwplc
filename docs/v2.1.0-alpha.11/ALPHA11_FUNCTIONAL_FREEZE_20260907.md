# Alpha11 — congelación funcional HMI

Fecha: 2026-09-07

Rama de trabajo:

```text
v2.1.0-alpha.11/feature/hmi-designer
```

## Estado

La funcionalidad de Alpha11 queda congelada. Desde este punto no se agregan features nuevas al HMI Designer; el trabajo restante corresponde al cierre técnico, precompilación, benchmark, documentación y publicación.

## Gates funcionales confirmados

```text
A11_HMI_DESIGNER_DESKTOP=PASS
A11_DECLARATIVE_TEXT_VALUE_BOOL_BAR=PASS
A11_MULTI_PAGE=PASS
A11_PIXELMAP_EDITOR=PASS
A11_PIXELMAP_RGB565=PASS
A11_PIXELMAP_LAYERS_AND_ONION_SKIN=PASS
A11_PIXELMAP_WORKBENCH=PASS
A11_PIXELMAP_PACKED_SPAN16_CODEGEN=PASS
A11_PIXELMAP_RUNTIME_UPLOAD=PASS
A11_PROJECT_JWHMI_SAVE=PASS
A11_PROJECT_JWHMI_REOPEN=PASS
A11_PROJECT_PIXELMAP_PERSISTENCE=PASS
A11_NATIVE_SKETCH_LINK=PASS
A11_HEADER_DIRECT_UPDATE=PASS
A11_FUNCTIONAL_FREEZE=PASS
```

Evidencia de usuario final:

- generación y subida física correctas al JWPLC Basic;
- cierre y reapertura del Designer conservando el pixel art;
- persistencia de PixelMaps dentro del proyecto `.jwhmi`;
- integración del sketch y actualización de `JWPLC_HMI_Generated.h` operativas.

## Convención de proyecto

```text
<Sketch>/
├── <Sketch>.ino
├── <Sketch>.jwhmi
└── JWPLC_HMI_Generated.h
```

Responsabilidades:

- `<Sketch>.jwhmi`: fuente editable/canónica de la interfaz;
- `JWPLC_HMI_Generated.h`: artefacto regenerable producido por el Designer;
- `<Sketch>.ino`: lógica de aplicación Arduino.

## PixelMap

Alpha11 incluye:

- PixelMaps declarativos RGB565;
- edición por capas;
- pincel y borrador con tamaño configurable;
- relleno, cuentagotas, línea y rectángulo;
- mover, duplicar, ocultar y onion skin de edición;
- undo/redo contextual;
- visibilidad real desde el runtime mediante `setPixelMapVisible()`;
- formato legacy `RGB565_RUN`;
- formato optimizado `PACKED_SPAN16` con fallback automático;
- generación C++ para proyectos con fields, PixelMaps o ambos.

La opacidad/onion skin del Designer es una ayuda de edición y no introduce alpha en RGB565 ni cambia el resultado generado.

## Pendientes de cierre

```text
A11_DISPLAY_PRECOMPILED_ARCHIVE=PENDING
A11_DISPLAY_SOURCE_PRECOMPILED_PARITY=PENDING
A11_FINAL_BUILD_SPEED_BENCHMARK=PENDING
A11_LIBRARY_READMES=PENDING
A11_REPOSITORY_README=PENDING
A11_RELEASE_CHECKLIST=PENDING
A11_PR_RELEASE_V2_1_X=PENDING
A11_PRERELEASE=PENDING
A11_RELEASE_MAIN_SYNC=PENDING
```

## Orden de cierre

```text
freeze funcional
-> regenerar libJWPLC_Display.a desde las TUs actuales
-> auditar source vs precompiled
-> benchmark final Basic/Core
-> actualizar README/documentación/checklist
-> PR técnico en español hacia release/v2.1.x
-> PreRelease v2.1.0-alpha.11
-> sincronización auditada release/v2.1.x -> main
-> verificar release/v2.1.x ancestro de main
```

No se modifican como efecto lateral:

- configuración final de FlashFreq;
- bootloader precompilado;
- política app-only por defecto;
- OTA;
- periféricos del autoload normal.
