# Alpha11 — Contrato de codegen sobre API pública JWPLC_Display

Fecha: 2026-09-05

## Objetivo

Fijar antes de A11-3/A11-4 qué debe generar **JWPLC HMI Designer** y qué queda bajo control manual del usuario.

Regla principal:

```text
DESIGNER_GENERATED_CODE_USES_PUBLIC_API=YES
DESIGNER_GENERATED_CODE_USES_INTERNAL_RUNTIME=NO
DESIGNER_GENERATED_CODE_USES_LEGACY_CALLBACKS=NO

DESIGNER_GENERATES_FIELD_STRUCTURES=YES
DESIGNER_GENERATES_VARIABLE_DECLARATIONS=YES
DESIGNER_GENERATES_HMI_REGISTRATION=YES
DESIGNER_GENERATES_DISPLAY_CONFIGURATION=YES

DESIGNER_GENERATES_JWPLC_UI_UPDATE=NO
USER_WRITES_JWPLC_UI_UPDATE=YES
```

El Designer define toda la estructura de la HMI, incluidos los **nombres y tipos de las variables HMI** configuradas visualmente. El usuario no debe volver a declarar esas variables manualmente.

La frontera manual empieza dentro de `jwplcUIUpdate()`: allí el usuario decide de dónde obtiene cada dato y cómo lo publica en el field correspondiente.

---

## 1. Límite de responsabilidad del Designer

El Designer genera:

```text
- IDs simbólicos de fields;
- declaraciones/definiciones de variables HMI;
- tipo C++ de cada variable HMI;
- capacidad de buffers de texto;
- JWPLC_UIField[];
- páginas;
- posición/tamaño;
- label/unit;
- formatos;
- estilos;
- colores;
- frames;
- rangos;
- textos BOOL;
- registro mediante JWPLC_Display.setFields(...);
- configuración USER/refresh/página inicial cuando corresponda.
```

El Designer **NO genera el cuerpo de**:

```cpp
extern "C" void jwplcUIUpdate()
```

Ni asume dentro de ese callback:

```text
- lecturas de sensores;
- IO;
- RTC;
- FRAM;
- Ethernet;
- Modbus;
- fórmulas de proceso;
- Ladder/OpenPLC;
- lógica de aplicación.
```

La frontera queda así:

```text
JWPLC HMI Designer
        ↓
variables HMI + field IDs + JWPLC_UIField[] + setup HMI
        ↓
---------------- FRONTERA ----------------
        ↓
usuario implementa jwplcUIUpdate()
        ↓
usuario alimenta las variables HMI y publica los fields
```

---

## 2. Variables HMI definidas desde la interfaz

Cada objeto dinámico debe poder definir desde el Designer, como mínimo:

```text
Nombre de variable
Tipo C++
Valor inicial / preview
```

Ejemplo visual:

```text
Field       : Temperatura
ID          : FIELD_TEMP
Variable    : temperatura
Tipo        : float
Preview     : 25.6
```

El codegen debe dejar la variable disponible al sketch sin que el usuario tenga que volver a declararla.

Ejemplo conceptual:

```cpp
extern float temperatura;
extern bool motorOn;
extern char estadoTexto[13];
extern float nivel;
```

con sus definiciones correspondientes en el archivo generado.

### Tipos V1 recomendados

```text
VALUE -> tipo numérico elegible por el usuario; default float
TEXT  -> buffer char[capacity + 1]
BOOL  -> bool
BAR   -> float
```

Para `TEXT`, la capacidad visual del field debe coincidir con el tamaño útil del buffer generado.

Si varios fields usan el mismo símbolo de variable, el Designer debe declararlo una sola vez y validar compatibilidad de tipo.

La forma exacta final de almacenamiento —variables individuales o un struct generado de datos HMI— se cerrará en A11-4. En ambos casos la regla es la misma: **la declaración pertenece al Designer; la adquisición/asignación del dato pertenece al usuario**.

---

## 3. API autorizada para definición de campos

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

## 4. API autorizada para registrar la HMI

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

## 5. IDs y variables como contrato con el usuario

El Designer genera IDs legibles y estables:

```cpp
enum HMIFieldId : uint8_t
{
    FIELD_TEMP = 1,
    FIELD_RUN = 2,
    FIELD_LEVEL = 3
};
```

Y las variables HMI configuradas visualmente:

```cpp
float temperatura = 0.0f;
bool motorOn = false;
float nivel = 0.0f;
```

De esta manera el usuario entra a `jwplcUIUpdate()` con todo el contrato ya preparado:

```text
FIELD_TEMP   <-> temperatura
FIELD_RUN    <-> motorOn
FIELD_LEVEL  <-> nivel
```

El Designer puede mostrar esta asociación en la interfaz, pero **no decide la fuente del dato**.

---

## 6. `jwplcUIUpdate()` pertenece al usuario

El Designer no crea ni regenera la función.

El usuario puede escribir, por ejemplo:

```cpp
extern "C" void jwplcUIUpdate()
{
    temperatura = obtenerTemperatura();
    motorOn = JWPLC_IO.readDO(0);
    nivel = nivelProceso;

    JWPLC_Display.setValue(FIELD_TEMP, temperatura);
    JWPLC_Display.setBool(FIELD_RUN, motorOn);
    JWPLC_Display.setBar(FIELD_LEVEL, nivel);
}
```

Lo importante es que:

- `temperatura`, `motorOn` y `nivel` ya fueron declaradas por el Designer;
- `FIELD_TEMP`, `FIELD_RUN` y `FIELD_LEVEL` ya fueron generados por el Designer;
- el usuario sólo completa la lógica de actualización dentro de `jwplcUIUpdate()`.

El Designer puede mostrar un **snippet sugerido** para ayudar al usuario, pero ese callback no forma parte de los archivos regenerables.

Regla:

```text
DESIGNER_GENERATES_VARIABLE_CONTRACT=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE_BODY=NO
USER_CONFIGURES_JWPLC_UI_UPDATE=YES
```

---

## 7. Callbacks

El codegen V1 no genera automáticamente `jwplcUIUpdate()` ni callbacks vacíos.

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

## 8. Regla sobre `JWPLC_Display.tft()`

`JWPLC_Display.tft()` sigue siendo una API pública válida para dibujo directo con Adafruit GFX, pero **no forma parte del codegen declarativo V1**.

```text
GFX_RAW_TOOL=DIAGNOSTIC_ONLY
GFX_RAW_GENERATED_AS_STANDARD_HMI=NO
```

La herramienta `Texto GFX RAW` de A11-2 existe para validar paridad de píxeles entre Designer y Adafruit GFX.

No debe confundirse con `TEXT field`.

---

## 9. Padding y fondo

La asimetría observada en A11-2 pertenece a la celda clásica GFX RAW de `6x8`.

Los fields declarativos usan actualmente en el runtime:

```text
FIELD_PADDING=3
FIELD_GAP=4
```

El runtime pinta primero el rectángulo completo del field con `background` y luego ubica `label/value/unit` dentro del field.

Para A11-3 el Designer debe reproducir estos valores exactamente.

Por ahora:

```text
PUBLIC_PADDING_PARAMETER_REQUIRED_FOR_V1=NO
PUBLIC_GAP_PARAMETER_REQUIRED_FOR_V1=NO
JWPLC_DISPLAY_API_CHANGE_FOR_A11_3=NO
```

Si A11-3 concluye que padding/gap deben ser editables, se abrirá una ampliación pública aditiva antes de A11-4.

```text
DESIGNER_PROPERTY_WITHOUT_PUBLIC_CODEGEN=FORBIDDEN
```

---

## 10. Forma objetivo del código generado

Ejemplo conceptual:

```cpp
#include <JWPLC_Display.h>

enum HMIFieldId : uint8_t
{
    FIELD_TEMP = 1,
    FIELD_RUN = 2
};

// Variables declaradas desde el Designer.
float temperatura = 0.0f;
bool motorOn = false;

static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UIValueField(
        FIELD_TEMP,
        JWPLC_UIRect(10, 30),
        JWPLC_UIText("Temp", "C"),
        JWPLC_UIValueFormat(3, 1, true, false),
        JWPLC_UIValueStyle(2, 1, true),
        0),

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

**El archivo regenerable del Designer no contiene `jwplcUIUpdate()`.**

El usuario completa esa parte en su sketch usando las variables e IDs ya generados.

---

## 11. Criterio para ampliar la API durante Alpha11

Se modifica la API pública únicamente cuando:

1. existe una propiedad necesaria del Designer;
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
DESIGNER_GENERATES_VARIABLE_DECLARATIONS=YES
DESIGNER_GENERATES_FIELD_STRUCTURES=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE=NO
USER_WRITES_JWPLC_UI_UPDATE=YES
USER_CONFIGURES_DYNAMIC_VALUES_IN_UPDATE=YES
API_CHANGE_REQUIRED_NOW=NO
NEXT=A11_2_PHYSICAL_GFX_PARITY_THEN_A11_3_FIELDS
```
