# Alpha11 — A11-4 Codegen integral

Fecha: 2026-09-05

## Objetivo

Validar el código C++ generado por JWPLC HMI Designer como artefacto final reutilizable en un sketch Arduino real, después de cerrar TEXT, VALUE, BOOL, BAR y páginas.

```text
A11_3E_MULTI_FIELD_PAGES=PASS
A11_4_CODEGEN=READY_TO_VALIDATE
PUBLIC_API_ONLY=REQUIRED
DIRECT_TFT_IN_GENERATED_CODE=FORBIDDEN
DESIGNER_GENERATES_JWPLC_UI_UPDATE=NO
```

## Frontera del código generado

El Designer debe generar:

```text
1. enum HMIFieldId
2. variables HMI
3. JWPLC_UIField HMI_FIELDS[]
4. jwplcHMISetup()
5. comentarios con setters públicos correspondientes
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

## Contrato esperado

Para una composición con TEXT + VALUE + BOOL + BAR en varias páginas:

```cpp
enum HMIFieldId : uint8_t
{
    FIELD_TEXT_1 = 1,
    FIELD_VALUE_2 = 2,
    FIELD_BOOL_3 = 3,
    FIELD_BAR_4 = 4
};

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
[ ] TEXT reserva capacity + 1 para terminador nulo.
[ ] Los setters comentados corresponden al tipo de field.
```

## Gate de compilación Arduino

El código generado se insertará sin modificaciones semánticas en un sketch real:

```cpp
#include <JWPLC_Display.h>

// <bloque generado por Designer>

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
