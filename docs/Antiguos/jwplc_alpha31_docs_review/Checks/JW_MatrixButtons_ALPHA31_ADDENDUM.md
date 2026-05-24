# JW_MatrixButtons - Addendum alpha31

Archivo destino sugerido:

```txt
README.md
```

Repositorio destino sugerido:

```txt
JW-Control/JW_MatrixButtons
```

## Nota opcional de integración JWPLC

Agregar después de la sección `¿Para qué sirve?`.


## Integración con JWPLC Basic

Dentro del package `JWPLC Basic`, esta librería puede ser usada como base para la botonera frontal del PLC mediante objetos globales del ecosistema JWPLC.

En uso normal del PLC, el usuario final no necesita escanear manualmente la matriz si trabaja con las APIs ya integradas del package. Sin embargo, `JW_MatrixButtons` sigue siendo útil para proyectos standalone, HMIs personalizadas o botoneras matriciales externas.

La recomendación para interfaces con varias pantallas se mantiene:

```cpp
clearPendingInput();
```

al cambiar de pantalla, entrar a edición, cerrar popups o volver a una vista anterior.


## Estado sugerido

## Estado

Documentación revisada para integración con:

```text
JWPLC Basic v2.0.0-alpha.31
```
```
