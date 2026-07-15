# Reglas de renderizado para pantallas USER del Logic Runtime

## Objetivo

Todas las vistas de `JWPLC_LogicRuntime_UI` deben mantener una interfaz fluida sobre la TFT ST7789 sin reconstruir la pantalla completa en cada refresco.

Estas reglas son obligatorias para las vistas actuales y futuras:

```text
RuntimeUIHome
RuntimeUIProgram
RuntimeUIBlocks
RuntimeUIStorage
RuntimeUIDiagnostics
RuntimeUIBlockEditor
RuntimeUIConfirmDialog
```

## Problema que se evita

El patrón siguiente produce parpadeo y tráfico SPI innecesario:

```cpp
void refresh()
{
    tft.fillRect(...);   // borra aunque el valor no cambió
    tft.print(...);      // vuelve a escribir lo mismo
}
```

Aunque la llamada sea rápida, existe un intervalo visible entre borrar y volver a dibujar. Si se repite cada 50 o 200 ms, el texto parece vibrar o parpadear.

## Arquitectura obligatoria de cada vista

Cada pantalla debe separar tres responsabilidades:

```text
enter()
├── dibuja estructura estática una sola vez
├── invalida la caché dinámica
├── dibuja el estado dinámico inicial
└── deja la caché válida

refresh()
├── procesa entradas
├── compara valores actuales contra la caché
├── actualiza únicamente campos modificados
└── aplica periodos separados según el tipo de dato

exit()
└── limpia solo el estado lógico temporal de la vista
```

`fillScreen()` solo se permite al entrar en una vista o durante una reconstrucción explícita mediante `forceRedraw()`.

## Contenido estático

Se dibuja una sola vez en `enter()`:

- fondo;
- marcos y paneles;
- divisores;
- título de la pantalla;
- etiquetas;
- iconos fijos;
- botones no seleccionados;
- instrucciones permanentes del pie de pantalla.

No debe volver a dibujarse desde el refresco periódico.

## Contenido dinámico

Cada valor dinámico debe conservar su copia anterior en una caché local de la vista.

Ejemplo:

```cpp
struct DynamicCache
{
    bool valid;
    RuntimeState runtimeState;
    uint32_t programId;
    uint32_t generation;
    uint16_t blockCount;
    uint32_t averageScanUs;
};
```

La actualización debe realizarse por diferencia:

```cpp
if (!cache.valid || currentState != cache.runtimeState)
{
    updateStateBadge(currentState);
    cache.runtimeState = currentState;
}
```

Consultar un valor en cada callback es válido. Escribir en la TFT cuando ese valor no cambió, no.

## Frecuencias recomendadas

No todos los datos requieren la misma frecuencia visual:

| Tipo de dato | Política recomendada |
|---|---|
| Selección de menú | inmediata, solo por evento de botón |
| RUN, READY, STOPPED, FAULT | al cambiar |
| Programa, Program ID, generación | al cambiar |
| Estado de almacenamiento y retentivos | al cambiar |
| Entradas y salidas | al cambiar |
| Tiempo de scan promedio/máximo | cada 500 a 1000 ms y solo si cambió |
| RTC segundos | cada segundo |
| RTC fecha | solo al cambiar el día |
| Marcos, etiquetas y títulos | solo en `enter()` |

El callback USER puede ejecutarse cada 50 ms para mantener una botonera ágil. Eso no significa que la TFT deba recibir escrituras cada 50 ms.

## Campos de texto de ancho fijo

Para valores variables se usa `updateTextField()`.

El helper:

1. limita el texto al ancho disponible;
2. rellena el resto con espacios;
3. configura color de texto y fondo;
4. imprime el campo completo en una sola pasada.

Ejemplo:

```cpp
updateTextField(tft,
                68,
                43,
                40,
                programName,
                COLOR_TEXT,
                COLOR_PANEL);
```

No se debe hacer primero un `fillRect()` periódico y después un `print()` del mismo dato.

## Encabezados

El encabezado se divide en:

```cpp
drawHeaderStatic(tft, "JWPLC LOGIC");
updateHeaderState(tft, "READY", COLOR_ACCENT);
```

`drawHeaderStatic()` se llama al entrar.

`updateHeaderState()` se llama únicamente cuando cambia el estado o cuando la caché está invalidada.

## Menús y selección

Al mover el cursor no se redibuja la matriz completa de botones.

Solo deben actualizarse:

```text
botón anteriormente seleccionado → estado normal
botón actualmente seleccionado   → estado seleccionado
```

El resto de botones permanece intacto.

## Redibujado completo

Una vista puede marcarse para reconstrucción cuando:

- se entra desde otra pantalla;
- cambia completamente el layout;
- se recupera el bus o la TFT tras un fallo;
- se llama explícitamente a `forceRedraw()`.

El flujo debe ser:

```cpp
invalidateCache();
drawStaticLayout();
updateAllDynamicFields(true);
drawMenu();
cache.valid = true;
```

No debe quedar una segunda reconstrucción pendiente después de `enter()`.

## Buffers y sprites

No se usará un framebuffer completo para la TFT actual:

```text
320 × 170 × 2 bytes = 108800 bytes
```

Ese costo de RAM no se justifica para pantallas mayormente estáticas.

Se prioriza:

```text
caché de valores
+ dirty fields
+ dirty rectangles pequeños
+ actualización por evento
```

Sprites o buffers parciales solo se evaluarán para elementos realmente animados, gráficas o transiciones que no puedan resolverse con campos sucios.

## Bus SPI

Las vistas USER dibujan dentro de los callbacks administrados por `JWPLC_Display`. En ese punto el package ya adquirió y configuró el bus SPI para la TFT.

Reglas:

- no adquirir nuevamente el bus desde una pantalla;
- no guardar una referencia global a `JWPLC_Display.tft()`;
- obtener la referencia local únicamente durante el dibujo;
- no ejecutar operaciones lentas de almacenamiento dentro del callback gráfico.

## Plantilla para una nueva pantalla

```cpp
void RuntimeUIView::enter()
{
    invalidateCache();
    drawStaticLayout();
    updateImmediateFields(true);
    updateSlowFields(true);
    drawSelection();
    cache.valid = true;
}

void RuntimeUIView::refresh(...)
{
    handleInput();
    updateImmediateFields(false);

    if (elapsed(now, lastSlowRefresh, 1000))
    {
        updateSlowFields(false);
    }
}
```

## Criterios de revisión para cada pantalla

Antes de aprobar una vista nueva debe comprobarse:

```text
[ ] No usa fillScreen() dentro del refresh normal.
[ ] Las etiquetas y marcos se dibujan solo en enter().
[ ] Cada dato dinámico tiene caché o condición de cambio.
[ ] Los datos lentos tienen un periodo visual propio.
[ ] La navegación redibuja solo selección anterior y actual.
[ ] El texto variable usa ancho fijo o limpia únicamente su región al cambiar.
[ ] Una entrada repetida a USER reconstruye la pantalla correctamente.
[ ] No hay parpadeo periódico con valores estables.
[ ] La interfaz no altera el estado lógico ni las salidas.
[ ] Se mide la latencia del scan cuando la vista se pruebe con un programa activo.
```

## Aplicación inicial

`RuntimeUIHome` es la primera vista migrada a estas reglas:

- encabezado estático separado de la insignia de estado;
- etiquetas estáticas dibujadas una sola vez;
- caché para runtime, programa, identidad, almacenamiento y retentivos;
- scan actualizado como máximo una vez por segundo;
- botones redibujados únicamente al cambiar la selección;
- campos de texto actualizados con ancho fijo;
- sin segunda reconstrucción inmediata después de `enter()`.
