# Alpha11 — Contrato de codegen sobre API pública JWPLC_Display

Fecha: 2026-09-05

## Objetivo

Fijar antes de A11-3/A11-4 qué API puede emitir **JWPLC HMI Designer** en código destinado al usuario.

La regla principal es:

```text
DESIGNER_GENERATED_CODE_USES_PUBLIC_API=YES
DESIGNER_GENERATED_CODE_USES_INTERNAL_RUNTIME=NO
DESIGNER_GENERATED_CODE_USES_LEGACY_CALLBACKS=NO
DESIGNER_STOPS_BEFORE_JWPLC_UI_UPDATE=YES
USER_OWNS_JWPLC_UI_UPDATE=YES
```

El Designer es un frontend de la API pública existente de `JWPLC_Display`; no debe generar un segundo runtime ni apropiarse de la lógica de actualización de variables del sketch.

---

## 1. Límite de responsabilidad del Designer

El Designer genera únicamente la **definición estructural y configuración de la HMI**:

```text
objetos visuales
páginas
posición/tamaño
label/unit
formatos
estilos
colores
frames
rangos
textos BOOL
registro de fields
configuración USER/refresh/página inicial cuando corresponda
```

El Designer **NO genera**:

```text
jwplcUIUpdate()
lecturas de variables del sketch
setValue(...)
setText(...)
setBool(...)
setBar(...)
lógica de sensores
lógica de proceso
lógica Ladder/OpenPLC
```

La frontera es deliberada:

```text
JWPLC HMI Designer
        ↓
field IDs + JWPLC_UIField[] + setup HMI
        ↓
---------------- FRONTERA ----------------
        ↓
usuario implementa jwplcUIUpdate()
        ↓
usuario decide qué variables alimentan cada field
```

Razón: el Designer conoce la presentación, pero no debe asumir cómo ni cuándo se adquieren o actualizan los datos de la aplicación.

---

## 2. API autorizada para definición de campos

El codegen V1 debe construir la HMI mediante los helpers públicos:

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
```

Y sus grupos públicos cuando sean necesarios:

```cpp
JWPLC_UIRect(...)
JWPLC_UIText(...)
JWPLC_UIValueFormat(...)
JWPLC_UIBoolText(...)
JWPLC_UIRange(...)
JWPLC_UIColors(...)

JWPLC_UIValueStyle(...)
JWPLC_UITextFieldStyle(...)
JWPLC_UIBoolStyle(...)
JWPLC_UIBarStyle(...)
```

El Designer no debe generar inicialización miembro-a-miembro de estructuras internas salvo que exista una propiedad pública que no pueda expresarse mediante los helpers anteriores.

---

## 3. API autorizada para registrar la HMI

La definición generada debe registrarse mediante:

```cpp
JWPLC_Display.setFields(fields, count);
```

Configuración pública relacionada:

```cpp
JWPLC_Display.setUserRefreshMode(...);
JWPLC_Display.setUserRefreshPeriodMs(...);
JWPLC_Display.setUserPage(...);
JWPLC_Display.setIdleWakeMode(...);
JWPLC_Display.setIdleReturnMode(...);
JWPLC_Display.setIdleTimeoutMs(...);
```

Sólo se emitirá cada llamada cuando corresponda a una opción configurada en el proyecto `.jwhmi`.

---

## 4. IDs públicos como contrato con el usuario

El Designer sí debe generar IDs legibles y estables para que el usuario los consuma luego desde `jwplcUIUpdate()`.

Ejemplo:

```cpp
enum HMIFieldId : uint8_t
{
    FIELD_TEMP = 1,
    FIELD_RUN = 2,
    FIELD_LEVEL = 3
};
```

Estos IDs son el punto de enlace entre el código generado y el código manual.

El Designer puede mostrar en su interfaz un nombre simbólico como:

```text
FIELD_TEMP
FIELD_RUN
FIELD_LEVEL
```

pero no debe convertirlo automáticamente en una lectura o expresión C++ de la aplicación.

---

## 5. `jwplcUIUpdate()` pertenece al usuario

El usuario implementa manualmente, cuando lo necesite:

```cpp
extern "C" void jwplcUIUpdate()
{
    JWPLC_Display.setValue(FIELD_TEMP, temperatura);
    JWPLC_Display.setBool(FIELD_RUN, motorOn);
    JWPLC_Display.setBar(FIELD_LEVEL, nivel);
}
```

El Designer no escribe ni regenera este callback.

Esto permite que el usuario decida libremente:

- qué variable usar;
- si usa una variable cacheada;
- si aplica escalado o conversión;
- si un valor viene de IO, RTC, FRAM, Ethernet, Modbus u otra lógica;
- cuándo actualizar un field;
- qué condiciones de aplicación deben existir antes de publicar un valor.

Regla:

```text
DESIGNER_GENERATES_DYNAMIC_BINDINGS=NO
USER_CONTROLS_DYNAMIC_BINDINGS=YES
```

El Designer puede, como ayuda visual futura, mostrar el **ID del field** y ejemplos de uso, pero dichos ejemplos no forman parte de los archivos generados.

---

## 6. Callbacks

El codegen V1 no necesita generar `jwplcUIUpdate()`.

Tampoco debe generar automáticamente callbacks vacíos sólo por existir la HMI.

Si más adelante una función visual estática requiere código adicional en:

```cpp
jwplcUIEnter()
jwplcUIPageEnter(uint8_t page)
jwplcUIExit()
```

se evaluará de forma explícita y únicamente mediante la API corta recomendada.

No se generarán como API principal:

```text
jwplcUserDisplayEnterCallback
jwplcUserDisplayRefreshNeededCallback
jwplcUserDisplayRefreshCallback
jwplcUserDisplayExitCallback
```

Los callbacks legacy se conservan únicamente por compatibilidad del package.

---

## 7. Regla sobre `JWPLC_Display.tft()`

`JWPLC_Display.tft()` sigue siendo una API pública válida para dibujo directo con Adafruit GFX, pero **no forma parte del codegen declarativo V1**.

En Alpha11:

```text
GFX_RAW_TOOL=DIAGNOSTIC_ONLY
GFX_RAW_GENERATED_AS_STANDARD_HMI=NO
```

La herramienta `Texto GFX RAW` de A11-2 existe para validar paridad de píxeles entre Designer y Adafruit GFX.

No debe confundirse con `TEXT field`.

Un `TEXT field` generado por el Designer termina en una definición:

```cpp
JWPLC_UITextField(...)
```

La actualización de su contenido mediante:

```cpp
JWPLC_Display.setText(...)
```

pertenece al usuario en `jwplcUIUpdate()` o en la lógica que corresponda.

---

## 8. Padding y fondo

La asimetría observada en A11-2 pertenece a la celda clásica GFX RAW de `6x8`.

Los fields declarativos usan actualmente en el runtime:

```text
FIELD_PADDING=3
FIELD_GAP=4
```

Además el runtime pinta primero el rectángulo completo del field con `background` y luego ubica `label/value/unit` dentro del field.

Por tanto, para A11-3 el Designer debe reproducir estos valores como parte del contrato del runtime actual.

### Decisión de API

Por ahora:

```text
PUBLIC_PADDING_PARAMETER_REQUIRED_FOR_V1=NO
PUBLIC_GAP_PARAMETER_REQUIRED_FOR_V1=NO
JWPLC_DISPLAY_API_CHANGE_FOR_A11_3=NO
```

Razón: V1 puede reproducir exactamente el runtime actual sin ampliar la API y preservando estabilidad.

Si durante A11-3 se decide que el usuario debe **editar** padding/gap, entonces se abrirá una ampliación pública aditiva antes de A11-4. No se permitirá que el Designer ofrezca una propiedad que no pueda expresarse en código público generado.

Regla:

```text
DESIGNER_PROPERTY_WITHOUT_PUBLIC_CODEGEN=FORBIDDEN
```

---

## 9. Forma objetivo del código generado

Ejemplo conceptual para una HMI con temperatura y estado:

```cpp
#include <JWPLC_Display.h>

enum HMIFieldId : uint8_t
{
    FIELD_TEMP = 1,
    FIELD_RUN = 2
};

static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_TEMP,
        JWPLC_UIRect(10, 30),
        JWPLC_UIText("Temp", "C"),
        JWPLC_UIValueFormat(3, 1, true, false),
        JWPLC_UIValueStyle(2, 1, true),
        0,
        JWPLC_UIColors(ST77XX_WHITE, ST77XX_CYAN, ST77XX_BLACK, ST77XX_WHITE)),

    JWPLC_UIBoolField(
        FIELD_RUN,
        JWPLC_UIRect(170, 30),
        JWPLC_UIText("Estado"),
        JWPLC_UIBoolText("STOP", "RUN"),
        JWPLC_UIBoolStyle(2, 1, true),
        0)
};

void jwplcHMISetup()
{
    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));

    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.setUserPage(0);
}
```

**El archivo generado termina ahí.**

El usuario, en su propio código, puede escribir después:

```cpp
extern "C" void jwplcUIUpdate()
{
    JWPLC_Display.setValue(FIELD_TEMP, temperatura);
    JWPLC_Display.setBool(FIELD_RUN, motorOn);
}
```

pero esa sección no pertenece al Designer ni debe ser sobrescrita por él.

---

## 10. Criterio para ampliar la API durante Alpha11

Se modifica la API pública únicamente cuando se cumplen las tres condiciones:

1. existe una propiedad visual necesaria del Designer;
2. el runtime puede soportarla de forma estable;
3. la propiedad no puede expresarse con la API pública actual.

Toda ampliación debe ser aditiva y conservar sketches Alpha8/Alpha10 ya validados.

```text
BREAK_EXISTING_DISPLAY_API=NO
ADD_PUBLIC_API_WHEN_REQUIRED=YES
```

---

## Estado

```text
A11_3_PUBLIC_API_CODEGEN_CONTRACT=PASS
DESIGNER_STOPS_BEFORE_JWPLC_UI_UPDATE=YES
USER_OWNS_JWPLC_UI_UPDATE=YES
DESIGNER_GENERATES_DYNAMIC_BINDINGS=NO
API_CHANGE_REQUIRED_NOW=NO
NEXT=A11_2_PHYSICAL_GFX_PARITY_THEN_A11_3_FIELDS
```
