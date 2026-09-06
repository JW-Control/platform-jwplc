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

La separación de responsabilidades es:

```text
JWPLC_HMI_Generated.h
  -> IDs
  -> variables HMI
  -> definiciones JWPLC_UIField[]
  -> páginas / geometría / estilos
  -> jwplcHMISetup()

Proyecto.ino
  -> setup()
  -> jwplcUIUpdate()
  -> sensores / entradas
  -> lógica de aplicación
  -> loop()
```

El header generado debe poder reemplazarse al regenerar la HMI sin mezclar código manual del usuario.

## Frontera del código generado

El Designer debe generar:

```text
1. #pragma once + include público JWPLC_Display.h
2. enum HMIFieldId
3. variables HMI
4. JWPLC_UIField HMI_FIELDS[]
5. jwplcHMISetup()
6. comentarios con setters públicos correspondientes
```

El Designer no debe generar:

```text
- cuerpo de jwplcUIUpdate()
- tft.*
- llamadas directas Adafruit_GFX
- lógica de aplicación
- lectura de sensores / entradas
- navegación manual de páginas
```

La lógica runtime pertenece al usuario:

```cpp
void jwplcUIUpdate()
{
    // El usuario actualiza sus variables y llama setters públicos.
}
```

## Organización por páginas

Para facilitar mantenimiento, tres secciones del header se agrupan con comentarios de página:

```cpp
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

// ...

// Setters públicos que corresponden a este diseño:
// Página 01 · Principal
// JWPLC_Display.setText(...);
// JWPLC_Display.setValue(...);

// Página 02 · Proceso
// JWPLC_Display.setBool(...);
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
    JWPLC_Display.setUserPageCount(N);
    JWPLC_Display.setUserPage(0);
}
```

## Reglas de validez

```text
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
[ ] HMIFieldId queda separado por comentarios de página.
[ ] Variables HMI quedan separadas por comentarios de página.
[ ] Setters comentados quedan separados por comentarios de página.
[ ] No contiene tft.
[ ] No contiene Adafruit_ST7789.
[ ] No contiene Adafruit_GFX.
[ ] No define jwplcUIUpdate().
[ ] Incluye todos los fields de todas las páginas.
[ ] Cada helper contiene pageId correcto.
[ ] setUserPageCount(N) coincide con cantidad de páginas.
[ ] setUserPage(0) está presente.
[ ] Todos los IDs son únicos.
[ ] Todas las variables son únicas.
[ ] Un intento manual de ID duplicado es rechazado y muestra página origen.
[ ] Un intento manual de variable duplicada es rechazado y muestra página origen.
[ ] TEXT reserva capacity + 1 para terminador nulo.
[ ] Los setters comentados corresponden al tipo de field.
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
    // lógica escrita manualmente por el usuario
}

void loop()
{
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
```

## Criterio de salida

```text
A11_4_CODEGEN_STATIC=PASS
A11_4_CODEGEN_COMPILE=PASS
A11_4_CODEGEN_PHYSICAL=PASS
A11_4_CODEGEN=PASS
NEXT=A11_5_PHYSICAL_PARITY
```
