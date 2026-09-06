# Alpha11 — A11-4 Codegen integral

Fecha: 2026-09-06

## Objetivo

Validar el código C++ generado por JWPLC HMI Designer como artefacto final reutilizable en un sketch Arduino real, después de cerrar TEXT, VALUE, BOOL, BAR y páginas.

```text
A11_3E_MULTI_FIELD_PAGES=PASS
A11_4_CODEGEN=READY_TO_VALIDATE
PUBLIC_API_ONLY=REQUIRED
DIRECT_TFT_IN_GENERATED_CODE=FORBIDDEN
DESIGNER_GENERATES_JWPLC_UI_UPDATE=NO
```

## Artefacto generado

El destino recomendado queda fijado como una pestaña/header independiente del sketch:

```text
JWPLC_HMI_Generated.h
```

El Designer agrega directamente:

```cpp
#pragma once
#include <JWPLC_Display.h>
```

La separación de responsabilidades queda fijada así:

```text
JWPLC_HMI_Generated.h
  -> HMIPageId
  -> HMIFieldId
  -> variables HMI
  -> definiciones JWPLC_UIField[]
  -> páginas / geometría / estilos
  -> jwplcHMISetup()

Proyecto.ino
  -> setup()
  -> loop(): lógica de aplicación, botonera, sensores, E/S, cálculos
  -> jwplcUIUpdate(): sincronización de variables hacia fields gráficos
```

El header generado debe poder reemplazarse al regenerar la HMI sin mezclar código manual del usuario.

## Frontera del código generado

El Designer debe generar:

```text
1. #pragma once + include público JWPLC_Display.h
2. enum HMIPageId
3. enum HMIFieldId
4. variables HMI
5. JWPLC_UIField HMI_FIELDS[]
6. jwplcHMISetup()
7. bloques de referencia con setters públicos correspondientes
```

El Designer no debe generar:

```text
- cuerpo de jwplcUIUpdate()
- cuerpo de loop()
- tft.*
- llamadas directas Adafruit_GFX
- lógica de aplicación
- lectura de sensores / entradas
- navegación manual de páginas
```

La lógica de aplicación pertenece al usuario y se recomienda ubicarla en `loop()`. `jwplcUIUpdate()` queda exclusivamente como capa de presentación/sincronización gráfica.

Ejemplo:

```cpp
void loop()
{
    if (!JWPLC_Display.isUserPageSelection() &&
        JWPLC_Display.userPage() == PAGE_PROCESO)
    {
        if (JWPLC_Buttons.pressed(BTN_UP))
            valorProceso += 1.0f;
    }
}

void jwplcUIUpdate()
{
    JWPLC_Display.setValue(FIELD_PROCESO, valorProceso);
}
```

## Refresh normal

Para HMI generada por Designer, el modo normal queda fijado explícitamente como periódico:

```cpp
JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);
```

`USER_REFRESH_ON_DEMAND` se conserva como modo opcional/avanzado para aplicaciones que quieran solicitar ciclos manualmente con `requestUserRefresh()`.

En modo periódico, la lógica de `loop()` puede limitarse a modificar variables HMI. `jwplcUIUpdate()` se ejecuta en los ciclos USER y los setters sólo marcan dirty/redibujan cuando el valor realmente cambia.

## Organización por páginas

Las secciones del header se agrupan con comentarios de página.

```cpp
enum HMIPageId : uint8_t
{
    PAGE_PRINCIPAL = 0,
    PAGE_PROCESO = 1,
    PAGE_DIAGNOSTICO = 2
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Principal
    FIELD_TEXT_1 = 1,
    FIELD_VALUE_2 = 2,

    // Página 02 · Proceso
    FIELD_BOOL_3 = 3,
};

// Variables HMI
// Página 01 · Principal
char texto1[13] = {};
float valor2 = 0.0f;

// Página 02 · Proceso
bool estado3 = false;
```

Los setters de referencia se generan como bloques `/* ... */` por página para que el usuario pueda copiar/descomentar el bloque sin quitar `//` línea por línea:

```cpp
// Setters públicos que corresponden a este diseño:

// Página 01 · Principal
/*
JWPLC_Display.setText(FIELD_TEXT_1, texto1);
JWPLC_Display.setValue(FIELD_VALUE_2, valor2);
*/

// Página 02 · Proceso
/*
JWPLC_Display.setBool(FIELD_BOOL_3, estado3);
*/
```

Los valores numéricos de `HMIFieldId` conservan el orden global del proyecto aunque la presentación se agrupe por página.

## Protección de identificadores

La creación/duplicación automática ya genera IDs y variables únicas. A11-4 agrega además protección para edición manual desde Inspector.

Se validan globalmente, entre todas las páginas:

```text
HMIFieldId / ID C++ = UNIQUE_GLOBAL
Variable vinculada  = UNIQUE_GLOBAL
```

La comparación se realiza sobre el símbolo C++ sanitizado. Por tanto, entradas diferentes que producirían el mismo identificador C++ también se consideran conflicto.

Ejemplo:

```text
FIELD-A -> FIELD_A
FIELD_A -> FIELD_A
```

Si el usuario intenta un duplicado, el cambio se rechaza y el Inspector muestra la ubicación del objeto existente:

```text
No se aplicó: ID C++ “FIELD_BOOL_3” ya existe en Página 02 · Proceso (BOOL 3).
```

El codegen realiza una segunda validación defensiva; si por algún estado heredado existiera un duplicado, marca el output como error de codegen en lugar de ocultar el conflicto.

## Contrato esperado

Para una composición con TEXT + VALUE + BOOL + BAR en varias páginas:

```cpp
#pragma once
#include <JWPLC_Display.h>

enum HMIPageId : uint8_t
{
    PAGE_PRINCIPAL = 0,
    PAGE_PROCESO = 1
};

enum HMIFieldId : uint8_t
{
    // Página 01 · Principal
    FIELD_TEXT_1 = 1,
    FIELD_VALUE_2 = 2,
    FIELD_BOOL_3 = 3,
    FIELD_BAR_4 = 4,
};

// Variables HMI
// Página 01 · Principal
char texto1[13] = {};
float valor2 = 0.0f;
bool estado3 = false;
float nivel4 = 0.0f;

static const JWPLC_UIField HMI_FIELDS[] =
{
    // helpers públicos JWPLC_UI*Field(...)
};

void jwplcHMISetup()
{
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);
    JWPLC_Display.setUserPageCount(N);
    JWPLC_Display.setUserPage(PAGE_PRINCIPAL);
}
```

## Reglas de validez

```text
PAGE_SYMBOLS_GENERATED=YES
FIELD_IDS_UNIQUE_GLOBAL=YES
VARIABLE_NAMES_UNIQUE_GLOBAL=YES
DUPLICATE_GUARD_IN_INSPECTOR=YES
DUPLICATE_GUARD_USES_SANITIZED_CPP_SYMBOL=YES
DUPLICATE_WARNING_SHOWS_PAGE=YES
PAGE_IDS_0_BASED=YES
VISIBLE_PAGE_NUMBERS_1_BASED=YES
TEXT_CAPACITY_BUFFER_PLUS_NULL=YES
VALUE_CPP_TYPE=float
BOOL_CPP_TYPE=bool
BAR_CPP_TYPE=float
COLORS=RGB565
PUBLIC_SETTERS_ONLY=YES
NORMAL_REFRESH_MODE=USER_REFRESH_PERIODIC
ON_DEMAND_MODE=OPTIONAL
APPLICATION_LOGIC_LOCATION=loop()
GRAPHIC_SYNC_LOCATION=jwplcUIUpdate()
```

## Matriz mínima de prueba

Proyecto de al menos tres páginas:

```text
Página 0 · Principal
  TEXT
  VALUE
  BOOL
  BAR

Página 1 · Proceso
  TEXT
  VALUE
  BAR

Página 2 · Diagnóstico
  BOOL
  VALUE
```

Debe incluir deliberadamente:

```text
INLINE + STACKED
LEFT + CENTER + RIGHT
frame on/off
varios colores RGB565
VALUE signed/unsigned
VALUE con decimales
BAR AUTO/FIJO
BOOL false/true text personalizados
```

## Gate estático

El bloque copiado desde `Código generado` debe cumplir:

```text
[ ] Empieza como header regenerable (#pragma once + JWPLC_Display.h).
[ ] Incluye HMIPageId con símbolos 0-based.
[ ] HMIFieldId queda separado por comentarios de página.
[ ] Variables HMI quedan separadas por comentarios de página.
[ ] Setters de referencia quedan en bloques /* ... */ separados por página.
[ ] jwplcHMISetup() fija USER_REFRESH_PERIODIC.
[ ] No contiene tft.
[ ] No contiene Adafruit_ST7789.
[ ] No contiene Adafruit_GFX.
[ ] No define jwplcUIUpdate().
[ ] No define loop().
[ ] Incluye todos los fields de todas las páginas.
[ ] Cada helper contiene pageId correcto.
[ ] setUserPageCount(N) coincide con cantidad de páginas.
[ ] setUserPage(PAGE_...) está presente.
[ ] Todos los IDs son únicos.
[ ] Todas las variables son únicas.
[ ] Un intento manual de ID duplicado es rechazado y muestra página origen.
[ ] Un intento manual de variable duplicada es rechazado y muestra página origen.
[ ] TEXT reserva capacity + 1 para terminador nulo.
[ ] Los setters corresponden al tipo de field.
```

## Gate de compilación Arduino

El header generado se usa sin modificaciones semánticas:

```cpp
#include "JWPLC_HMI_Generated.h"

void setup()
{
    jwplcHMISetup();
    JWPLC_Display.enterUserUI();
}

void jwplcUIUpdate()
{
    // Sólo sincronización gráfica.
}

void loop()
{
    // Lógica de aplicación.
}
```

Criterio:

```text
COMPILE=PASS
LINK=PASS
NO_UNDEFINED_REFERENCES=YES
NO_DIRECT_TFT_REQUIRED=YES
```

## Gate físico

Después de compilar/subir:

```text
1. TEXT se actualiza con setText().
2. VALUE se actualiza con setValue().
3. BOOL se actualiza con setBool().
4. BAR se actualiza con setBar().
5. Navegación NN/TT funciona con botonera física.
6. Cada página muestra sólo sus fields.
7. No aparecen restos visuales al cambiar página.
8. El sketch del usuario conserva control de botones dentro de PAGE_CONTENT.
9. La lógica en loop() modifica variables y se refleja vía refresh periódico + jwplcUIUpdate().
```

## Criterio de salida

```text
A11_4_CODEGEN_STATIC=PASS
A11_4_CODEGEN_COMPILE=PASS
A11_4_CODEGEN_PHYSICAL=PASS
A11_4_CODEGEN=PASS
NEXT=A11_5_PHYSICAL_PARITY
```
