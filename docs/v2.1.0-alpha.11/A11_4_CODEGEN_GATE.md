# Alpha11 — A11-4 Codegen integral

Fecha: 2026-09-06

## Estado

```text
A11_3E_MULTI_FIELD_PAGES=PASS
A11_4_CODEGEN_STATIC=PASS
A11_4_CODEGEN_COMPILE=PASS
A11_4_CODEGEN_PHYSICAL=PASS
A11_4_CODEGEN=PASS
PUBLIC_API_ONLY=PASS
DIRECT_TFT_IN_GENERATED_CODE=NO
DESIGNER_GENERATES_JWPLC_UI_UPDATE=YES
NEXT=A11_5_PHYSICAL_PARITY
```

## Objetivo

Validar el código C++ generado por JWPLC HMI Designer como artefacto reutilizable en un sketch Arduino real, después de cerrar TEXT, VALUE, BOOL, BAR y páginas.

## Artefacto generado

El destino recomendado queda fijado como una pestaña/header independiente:

```text
JWPLC_HMI_Generated.h
```

El archivo incluye:

```cpp
#pragma once
#include <JWPLC_Display.h>
```

Debe incluirse desde el `.ino` principal del proyecto.

## Separación de responsabilidades

```text
JWPLC_HMI_Generated.h
  -> HMIPageId
  -> HMIFieldId
  -> variables HMI
  -> definiciones JWPLC_UIField[]
  -> páginas / geometría / estilos
  -> jwplcHMISetup()
  -> jwplcUIUpdate() autogenerado

Proyecto.ino
  -> setup()
  -> loop(): lógica de aplicación, botonera, sensores, E/S y cálculos
```

La regla de Alpha11 queda fijada así:

```text
loop()          = lógica del programa
jwplcUIUpdate() = sincronización gráfica autogenerada
```

El usuario modifica variables HMI desde su lógica. El Designer se encarga de sincronizar la página activa con los setters públicos correspondientes.

## `jwplcUIUpdate()` autogenerado

El Designer genera un `switch` por `HMIPageId` para evitar ejecutar setters de páginas que no están visibles.

Ejemplo:

```cpp
void jwplcUIUpdate()
{
    switch (JWPLC_Display.userPage())
    {
    case PAGE_PRINCIPAL:
        // Página 01 · Principal
        JWPLC_Display.setText(FIELD_TEXT_1, texto1);
        JWPLC_Display.setValue(FIELD_VALUE_2, valor2);
        JWPLC_Display.setBool(FIELD_BOOL_3, estado3);
        break;

    case PAGE_PROCESO:
        // Página 02 · Proceso
        JWPLC_Display.setBar(FIELD_BAR_4, nivel4);
        break;

    default:
        break;
    }
}
```

Esto mantiene fuera del ciclo normal los setters de páginas inactivas. Al entrar en otra página, el siguiente ciclo USER sincroniza sus variables actuales y el motor dirty-region decide qué regiones requieren dibujo.

## Lógica del usuario

La lógica permanece en `loop()`.

```cpp
void loop()
{
    if (!JWPLC_Display.isUserPageSelection() &&
        JWPLC_Display.userPage() == PAGE_PROCESO)
    {
        if (JWPLC_Buttons.pressed(BTN_UP))
            nivel4 += 1.0f;

        if (JWPLC_Buttons.pressed(BTN_DOWN))
            nivel4 -= 1.0f;
    }
}
```

La HMI no modifica ni limita silenciosamente variables de proceso. Por ejemplo, `JWPLC_UIRange(min,max)` define el rango visual de BAR; cualquier saturación de la variable pertenece a la lógica del usuario.

## Refresh normal

Para HMI generada por Designer, el modo normal queda explícitamente periódico:

```cpp
JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);
```

`USER_REFRESH_ON_DEMAND` se conserva como opción avanzada.

En modo periódico, `loop()` puede limitarse a modificar variables. `jwplcUIUpdate()` se ejecuta en los ciclos USER y los setters sólo marcan dirty/redibujan cuando el valor realmente cambia.

## Organización por páginas

El header agrupa IDs y variables con comentarios de página:

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
    FIELD_BOOL_3 = 3
};

// Variables HMI
// Página 01 · Principal
char texto1[13] = {};
float valor2 = 0.0f;

// Página 02 · Proceso
bool estado3 = false;
```

`jwplcUIUpdate()` usa los mismos símbolos de página en su `switch`.

## Protección de identificadores

La creación, duplicación y edición manual mantienen unicidad global entre páginas:

```text
HMIFieldId / ID C++ = UNIQUE_GLOBAL
Variable vinculada  = UNIQUE_GLOBAL
```

La comparación se realiza sobre el símbolo C++ sanitizado. Si existe un conflicto, el Inspector rechaza el cambio e indica página y objeto de origen.

Validación física/UI:

```text
ID_CPP_DUPLICATE_GUARD=PASS
VARIABLE_CPP_DUPLICATE_GUARD=PASS
DUPLICATE_WARNING_INCLUDES_PAGE=PASS
SANITIZED_CPP_SYMBOL_COLLISION_GUARD=PASS
```

## Contrato generado validado

El Designer genera:

```text
1. #pragma once + JWPLC_Display.h
2. HMIPageId
3. HMIFieldId
4. variables HMI
5. HMI_FIELDS[]
6. jwplcHMISetup()
7. USER_REFRESH_PERIODIC
8. setUserPageCount(N)
9. setUserPage(PAGE_...)
10. jwplcUIUpdate() con switch de página activa
```

El Designer no genera:

```text
- loop()
- setup() de aplicación
- lógica de sensores / E/S / Modbus
- lógica de botones del proceso
- tft.*
- llamadas directas Adafruit_GFX/ST7789
```

## Gate de compilación Arduino

Uso validado:

```cpp
#include "JWPLC_HMI_Generated.h"

void setup()
{
    jwplcHMISetup();
    JWPLC_Display.enterUserUI();
}

void loop()
{
    // Lógica de aplicación.
}
```

No existe una segunda definición manual de `jwplcUIUpdate()` en el `.ino`, porque el header generado aporta la implementación fuerte que sustituye al hook weak de la librería.

Resultado:

```text
COMPILE=PASS
LINK=PASS
NO_DUPLICATE_JWPLC_UI_UPDATE=YES
NO_UNDEFINED_REFERENCES=YES
NO_DIRECT_TFT_REQUIRED=YES
```

## Gate físico

Se verificó con HMI real generada por Designer:

```text
TEXT_VARIABLE_UPDATE=PASS
VALUE_VARIABLE_UPDATE=PASS
BOOL_VARIABLE_UPDATE=PASS
BAR_VARIABLE_UPDATE=PASS
ACTIVE_PAGE_SWITCH=PASS
PAGE_NAVIGATION_NN_TT=PASS
INDICATOR_DIRTY_ONLY=PASS
ESC_PAGE_CONTENT_TO_SELECT=PASS
LOOP_LOGIC_TO_PERIODIC_HMI=PASS
```

Durante este gate se reprodujo y corrigió además un problema intermitente de robustez de botonera bajo `loop()` intensivo con `pressed()`. El cierre físico está documentado en:

```text
docs/v2.1.0-alpha.11/A11_BUTTON_ROBUSTNESS_GATE.md
```

## Reglas de validez cerradas

```text
PAGE_SYMBOLS_GENERATED=YES
FIELD_IDS_UNIQUE_GLOBAL=YES
VARIABLE_NAMES_UNIQUE_GLOBAL=YES
PAGE_IDS_0_BASED=YES
VISIBLE_PAGE_NUMBERS_1_BASED=YES
PUBLIC_SETTERS_ONLY=YES
NORMAL_REFRESH_MODE=USER_REFRESH_PERIODIC
ON_DEMAND_MODE=OPTIONAL
APPLICATION_LOGIC_LOCATION=loop()
GRAPHIC_SYNC_LOCATION=JWPLC_HMI_Generated.h::jwplcUIUpdate()
ACTIVE_PAGE_SWITCH=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE=YES
```

## Criterio de salida

```text
A11_4_CODEGEN_STATIC=PASS
A11_4_CODEGEN_COMPILE=PASS
A11_4_CODEGEN_PHYSICAL=PASS
A11_4_CODEGEN=PASS
NEXT=A11_5_PHYSICAL_PARITY
```
