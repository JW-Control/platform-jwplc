# Prueba física — Runtime UI Home v0.1

## Estado

```text
IMPLEMENTACIÓN PREPARADA
COMPILACIÓN Y VALIDACIÓN VISUAL PENDIENTES
```

## Ejemplo

```text
JWPLC_LogicRuntime_UI_Home
```

## Seguridad

- inicializa la E/S base mediante `runtime.begin()`;
- no carga ningún programa lógico;
- no contiene bloques `DigitalOutput`;
- no escribe ni formatea la FRAM;
- `storage().begin()` solo inspecciona el mapa existente;
- Q0 debe permanecer apagado.

## Flujo esperado

1. El sketch inicia en la pantalla `IDLE` normal.
2. `RUN` debe quedar apagado porque el runtime está en `READY`.
3. `ERR` debe quedar apagado: una FRAM sin formato no es una falla crítica.
4. El monitor de entradas y salidas de `IDLE` debe continuar funcionando.
5. Al pulsar cualquier botón se entra en la vista `USER`.
6. Debe mostrarse el encabezado `JWPLC LOGIC` y el estado `READY`.
7. Sin programa persistente debe mostrarse:

```text
Programa: SIN PROGRAMA
ID / Gen: SIN IDENTIDAD
Bloques: 0
```

8. Las flechas deben mover el selector entre:

```text
PROGRAMA
BLOQUES
MEMORIA
DIAGNOSTICO
```

9. `OK` debe mostrar durante aproximadamente 1.5 segundos la sección seleccionada.
10. `ESC` debe regresar a `IDLE`.
11. Una segunda entrada a `USER` debe reconstruir correctamente la vista.

## Monitor serial esperado

```text
JWPLC Logic Runtime UI - pantalla principal USER
IDLE conserva monitor I/O y LEDs del package.
Pulsa cualquier boton para entrar a USER.

Storage: OK
Runtime: OK
Runtime UI: OK

En USER: flechas mueven el selector, OK confirma y ESC vuelve a IDLE.
JWPLC_Display inicializado
```

El orden exacto de `JWPLC_Display inicializado` puede variar porque el display pertenece al autoload normal del package.

## Datos que deben registrarse

```text
Flash utilizada
RAM global utilizada
RAM restante
fotografía de IDLE
fotografía de USER
resultado de cada botón
retorno USER → IDLE
```

## Criterio de aprobación

```text
COMPILA
IDLE SIN REGRESIÓN
USER VISIBLE
BOTONERA OPERATIVA
ESC RETORNA A IDLE
FRAM SIN ESCRITURAS
Q0 APAGADAS
```
