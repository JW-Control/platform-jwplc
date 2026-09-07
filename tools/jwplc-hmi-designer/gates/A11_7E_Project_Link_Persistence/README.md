# A11-7E · Proyecto, vínculo y persistencia HMI

Estado: **PENDING_USER_GATE**

## Objetivo

Estabilizar el contrato de archivos del JWPLC HMI Designer y evitar pérdida de PixelMaps al guardar/abrir proyectos.

## Contrato de archivos

- `<sketch>.jwhmi`: fuente editable/canónica del diseño HMI.
- `<sketch>.ino`: lógica de aplicación Arduino.
- `JWPLC_HMI_Generated.h`: artefacto C++ autogenerado por el Designer; `Actualizar HMI` puede reemplazarlo.

## Bugs observados

### 1. Actualizar HMI deshabilitado tras reiniciar

En Electron, `designer-native-project.js` restaura correctamente el sketch persistido, pero `designer-project.js` también intenta restaurar un `FileSystemHandle` web. La restauración web puede finalizar después y deshabilitar `updateHmiButton` aunque el vínculo nativo siga válido.

### 2. Header Pixel-only marcado como inválido

`generatedHeaderText()` ejecutaba `JWPLCHMICodegen.refresh()` y validaba inmediatamente `codeOutput`. En un proyecto con 0 fields + PixelMaps, el header completo se termina de formar en el pipeline PixelMap después del refresh base, por lo que la validación podía leer temporalmente el stub sin `#pragma once`.

### 3. `.jwhmi` no persistía PixelMaps

La serialización Alpha11 original guardaba páginas y fields, pero no `JWPLCHMIPixelMaps.export()`. Los proyectos guardados antes de este gate no contienen el pixel art aunque exista en memoria durante la sesión.

## Corrección

Módulo `designer-project-integration-fix.js`:

- extiende la serialización pública con `pixelMaps`;
- restaura `pixelMaps` mediante `JWPLCHMIPixelMaps.import()`;
- conserva compatibilidad con proyectos previos sin `pixelMaps`;
- usa un flujo de apertura que pasa por la API pública corregida;
- reafirma el estado nativo del sketch tras la carrera web/native;
- reemplaza `Actualizar HMI` en Electron con una generación estable;
- fuerza `JWPLCHMIPixelStability.ensurePixelCode()` y `JWPLCHMIPixelCodegenGuard.apply()` antes de validar/escribir el header;
- mantiene `JWPLC_HMI_Generated.h` como artefacto regenerable.

## Gate de usuario

1. Abrir el Designer con un sketch nativo previamente vinculado.
2. Verificar que `Actualizar HMI` esté habilitado sin pulsar primero `Sketch: ...`.
3. Crear/editar un PixelMap.
4. Guardar `<sketch>.jwhmi`.
5. Cerrar completamente el Designer.
6. Reabrir el `.jwhmi` y verificar que el PixelMap reaparezca pixel-perfect.
7. Pulsar `Generar C++` y verificar un único `HMIPixelMapId`.
8. Pulsar `Actualizar HMI` y aceptar reemplazo si corresponde.
9. Verificar que `JWPLC_HMI_Generated.h` tenga contenido válido y compile.

## Compatibilidad

Los `.jwhmi` Alpha11 previos sin propiedad `pixelMaps` siguen siendo válidos; se abren con una lista PixelMap vacía. No es posible recuperar desde ellos PixelMaps que nunca fueron serializados.
