# Reglas de renderizado para pantallas USER del Logic Runtime

## Objetivo

Todas las vistas de `JWPLC_LogicRuntime_UI` deben mantener una interfaz fluida sobre la TFT ST7789, sin reconstruir periódicamente contenido estable y sin hacer que la carga inicial se perciba innecesariamente lenta.

Estas reglas son obligatorias para:

```text
RuntimeUIHome
RuntimeUIProgram
RuntimeUIBlocks
RuntimeUIStorage
RuntimeUIDiagnostics
RuntimeUIBlockEditor
RuntimeUIConfirmDialog
```

## Dos problemas distintos

### Parpadeo periódico

Ocurre cuando el callback borra y vuelve a imprimir un dato aunque no haya cambiado:

```cpp
void refresh()
{
    tft.fillRect(...);
    tft.print(...);
}
```

La solución es una caché de valores y actualización por diferencia.

### Construcción inicial lenta

Puede ocurrir incluso sin parpadeo cuando una vista nueva envía demasiadas operaciones pequeñas a la TFT.

El caso detectado en v0.2.0 fue imprimir campos de 30 a 51 columnas completando el ancho sobrante con espacios. Cada espacio se procesaba como otro glifo, aunque su única función fuera limpiar el campo.

Desde v0.2.1 se usa:

```text
limpieza rectangular continua
+ impresión transparente solo de caracteres útiles
```

## Arquitectura obligatoria de cada vista

```text
enter()
├── invalida la caché dinámica
├── dibuja estructura estática una sola vez
├── dibuja el estado dinámico inicial
├── dibuja selección inicial
└── deja la caché válida

refresh()
├── procesa entradas
├── compara valores actuales contra la caché
├── actualiza únicamente campos modificados
└── aplica periodos distintos según el tipo de dato

exit()
└── limpia solo estado lógico temporal de la vista
```

`fillScreen()` solo se permite al entrar en una vista o durante una reconstrucción explícita mediante `forceRedraw()`.

## Contenido estático

Se dibuja una sola vez en `enter()`:

- fondo;
- marcos y paneles;
- divisores;
- título;
- etiquetas;
- iconos fijos;
- botones;
- instrucciones permanentes.

El texto estático debe imprimirse en modo transparente sobre una región previamente pintada:

```cpp
tft.setTextColor(COLOR_TEXT);
tft.print("PROGRAMA");
```

No debe utilizar fondo opaco carácter por carácter cuando la región ya tiene su color definitivo.

## Contenido dinámico

Cada dato conserva una copia anterior:

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

Actualización por diferencia:

```cpp
if (!cache.valid || currentState != cache.runtimeState)
{
    updateStateBadge(currentState);
    cache.runtimeState = currentState;
}
```

Consultar un estado en cada callback es válido. Escribir en la TFT cuando no cambió, no.

## Campos de texto variables

Para valores variables se usa `updateTextField()`:

```cpp
updateTextField(tft,
                68,
                43,
                40,
                programName,
                COLOR_TEXT,
                COLOR_PANEL);
```

El helper realiza:

1. limita el texto al número de columnas disponible;
2. limpia toda la región mediante un `fillRect()` continuo;
3. imprime únicamente los caracteres útiles en modo transparente.

No rellena el campo enviando espacios como glifos.

Esta limpieza solo se permite cuando la caché confirmó que el valor cambió. No debe llamarse periódicamente sobre datos estables.

## Encabezados

El encabezado se divide en:

```cpp
drawHeaderStatic(tft, "JWPLC LOGIC");
updateHeaderState(tft, "READY", COLOR_ACCENT);
```

- `drawHeaderStatic()` se llama al entrar;
- `updateHeaderState()` se llama al cambiar el estado;
- la insignia limpia su propia región y luego imprime únicamente el texto útil.

## Menús

Al mover el cursor solo se actualizan:

```text
selección anterior -> estado normal
selección nueva    -> estado seleccionado
```

No se redibuja la matriz completa.

Los textos de botones se imprimen de forma transparente porque el fondo del botón ya fue pintado.

## Frecuencias visuales

| Tipo de dato | Política |
|---|---|
| Selección de menú | inmediata, por evento |
| READY, RUNNING, STOPPED, FAULT | al cambiar |
| Programa, ID y generación | al cambiar |
| Storage y retentivos | al cambiar |
| Entradas y salidas | al cambiar |
| Scan promedio/máximo | cada 500–1000 ms y solo si cambió |
| RTC segundos | cada segundo |
| Fecha | al cambiar el día |
| Marcos, títulos y etiquetas | solo en `enter()` |

El callback USER puede ejecutarse cada 50 ms para mantener la botonera ágil. Eso no autoriza escrituras TFT cada 50 ms.

## Redibujado completo

Una reconstrucción completa está permitida cuando:

- se entra desde otra vista;
- cambia el layout completo;
- se recupera la TFT o el bus;
- se llama a `forceRedraw()`.

Flujo:

```cpp
invalidateCache();
drawStaticLayout();
updateAllDynamicFields(true);
drawMenu();
cache.valid = true;
```

No debe quedar una segunda reconstrucción pendiente después de `enter()`.

## Framebuffer y sprites

No se usa framebuffer completo:

```text
320 × 170 × 2 bytes = 108800 bytes
```

Se prioriza:

```text
caché de valores
+ dirty fields
+ rectángulos pequeños
+ actualización por evento
```

Sprites parciales solo se evaluarán para gráficas o animaciones que no puedan resolverse con campos sucios.

## Bus SPI

Las vistas dibujan dentro de callbacks administrados por `JWPLC_Display`, donde el bus TFT ya está adquirido.

Reglas:

- no adquirir nuevamente el bus desde la vista;
- no guardar referencias globales a `JWPLC_Display.tft()`;
- obtener la referencia local durante el dibujo;
- no ejecutar FRAM, SD, Ethernet ni operaciones largas dentro del callback;
- registrar acciones y procesarlas luego desde `JWPLC_LogicRuntime_UI.update()`.

## Plantilla

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

## Criterios de revisión

```text
[ ] No usa fillScreen() durante refresh normal.
[ ] Marcos y etiquetas solo se dibujan en enter().
[ ] Cada dato dinámico tiene caché o condición de cambio.
[ ] Los datos lentos tienen periodo propio.
[ ] Solo se redibujan selección anterior y nueva.
[ ] Los campos variables limpian su región únicamente al cambiar.
[ ] No se envían columnas vacías como glifos.
[ ] Los textos estáticos usan impresión transparente.
[ ] Una entrada repetida reconstruye la pantalla correctamente.
[ ] No existe parpadeo periódico con valores estables.
[ ] La carga inicial no presenta pausas largas entre secciones.
[ ] La interfaz no altera lógica ni salidas.
[ ] Las acciones de almacenamiento se procesan fuera del callback TFT.
```

## Aplicación actual

`RuntimeUIHome` y `RuntimeUIProgram` aplican:

- dirty rendering;
- paleta JW Control;
- campos dinámicos por diferencia;
- navegación por evento;
- textos estáticos transparentes;
- campos variables con limpieza continua y texto útil;
- acciones diferidas fuera del callback gráfico.

La optimización de campos y textos transparentes se incorporó en `JWPLC_LogicRuntime_UI 0.2.1`.