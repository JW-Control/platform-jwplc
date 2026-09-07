# A11-7D · Fix de enum HMIPixelMapId duplicado

## Estado

`PENDING_USER_COMPILE_GATE`

## Síntoma observado

Al usar `Generar C++` y/o `Actualizar HMI`, el header podía contener dos definiciones consecutivas de:

```cpp
enum HMIPixelMapId : uint8_t
{
    PIXEL_1 = 0
};
```

Arduino IDE fallaba con:

```text
Compilation error: multiple definition of 'enum HMIPixelMapId'
```

## Causa

El pipeline A11-7D tiene varias capas de postprocesado:

- `designer-pixelmap-stability.js` genera los IDs públicos de PixelMap.
- `designer-pixelmap-optimizer.js` transforma el bloque a `PACKED_SPAN16` cuando conviene.
- el codegen base y la integración nativa actualizan el texto final en tiempos diferentes.

En ciertas secuencias, más de una capa preservaba/agregaba `HMIPixelMapId`.

## Corrección

Se añade `designer-pixelmap-codegen-guard.js` como último guard de Pixel codegen.

Reglas:

1. `designer-pixelmap-stability.js` es la fuente canónica del enum.
2. Antes de entregar un bloque PixelMap, el guard elimina todas las definiciones previas de `HMIPixelMapId` y reinyecta una sola definición canónica.
3. El `codeOutput` final se normaliza mediante `MutationObserver`, cubriendo tanto `Generar C++` como `Actualizar HMI`.
4. Si la capa stability todavía no está lista durante boot, el guard no modifica el header.
5. El comportamiento debe ser idempotente al regenerar repetidamente.

## Gate esperado

Con uno o más PixelMaps:

- `enum HMIPixelMapId` aparece exactamente una vez.
- `JWPLC_Display.setPackedPixelMaps(...)` o el fallback `setPixelMaps(...)` aparece una sola vez.
- pulsar `Generar C++` repetidamente no duplica bloques.
- `Actualizar HMI` escribe el mismo header normalizado.
- Arduino IDE compila sin `multiple definition`.

No se marca PASS hasta validar compilación de usuario.
