# Alpha11 — Semántica del Inspector HMI

Fecha: 2026-09-05

## Objetivo

Evitar ambigüedad entre metadatos del Designer, símbolos C++, variables de aplicación y texto visible en la TFT antes de extender el mismo patrón a `BOOL` y `BAR`.

La organización anterior agrupaba `Nombre`, `ID simbólico`, `Variable HMI`, `Valor de preview`, `Label` y `Unidad` bajo secciones que mezclaban responsabilidades.

## Estructura adoptada

```text
Identidad
├─ Nombre del objeto
└─ ID C++ del campo

Vinculación de datos
├─ Variable vinculada (C++)
├─ Tipo C++
└─ Capacidad              [cuando aplica]

Contenido
├─ Etiqueta visible
├─ Valor de prueba
└─ Unidad

Formato numérico          [VALUE]
Geometría
Tipografía
Apariencia
Contrato C++
```

## Significado de cada término

### Nombre del objeto

Nombre amigable usado únicamente por el Designer y por la lista lateral `Objetos`.

```text
Ejemplo: Temperatura reactor
```

No se dibuja en la TFT y no forma parte del contrato público de `JWPLC_Display`.

### ID C++ del campo

Identificador generado dentro de `HMIFieldId`.

```cpp
FIELD_TEMP
```

Se utiliza con los setters públicos:

```cpp
JWPLC_Display.setValue(FIELD_TEMP, temperatura);
```

### Variable vinculada (C++)

Variable declarada por el Designer y posteriormente alimentada por el usuario desde su lógica.

```cpp
float temperatura = 0.0f;
```

El Designer no genera `jwplcUIUpdate()`.

### Etiqueta visible

Texto fijo que sí forma parte del field y se muestra en la TFT.

```text
Temp
Estado
Nivel
```

Corresponde al texto declarativo utilizado por `JWPLC_UIText(...)`.

### Valor de prueba

Valor utilizado exclusivamente para el preview del Designer.

```text
READY
25.6
68
```

No crea lógica de actualización ni reemplaza el valor que el usuario establezca durante ejecución.

### Unidad

Texto opcional asociado al field.

```text
C
%
V
bar
```

## Regla UX

No reutilizar `Nombre` como sinónimo de `Label` ni `Variable`.

```text
OBJECT_NAME != VISIBLE_LABEL
FIELD_ID    != LINKED_VARIABLE
PREVIEW     != RUNTIME_VALUE
```

La interfaz debe comunicar estas diferencias mediante terminología estable y tooltips breves.

## Aplicación en A11-3A / A11-3B

La estructura se aplica de manera común a `TEXT` y `VALUE`.

Para `TEXT`:

```text
Vinculación de datos:
  Variable vinculada (C++)
  Tipo C++ = char[]
  Capacidad

Contenido:
  Etiqueta visible
  Valor de prueba
  Unidad
```

Para `VALUE`:

```text
Vinculación de datos:
  Variable vinculada (C++)
  Tipo C++ = float

Contenido:
  Etiqueta visible
  Valor de prueba
  Unidad

Formato numérico:
  Dígitos enteros
  Decimales
  Permitir signo
  Ceros a la izquierda
```

## Regla para gates siguientes

`BOOL` y `BAR` deben reutilizar esta misma semántica y sólo agregar las propiedades específicas de su tipo.

```text
INSPECTOR_SEMANTICS_COMMON=YES
BOOL_REUSES_IDENTITY_BINDING_CONTENT=YES
BAR_REUSES_IDENTITY_BINDING_CONTENT=YES
SECOND_TERMINOLOGY_MODEL=NO
```
