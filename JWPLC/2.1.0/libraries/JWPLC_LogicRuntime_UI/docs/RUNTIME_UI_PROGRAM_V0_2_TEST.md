# Prueba física — Runtime UI Programa v0.2

## Estado

```text
IMPLEMENTACIÓN PREPARADA
COMPILACIÓN Y VALIDACIÓN EN TFT PENDIENTES
```

## Alcance

Esta etapa agrega la primera navegación funcional dentro de `USER`:

```text
HOME
└── PROGRAMA
    ├── PREPARAR
    ├── RUN
    ├── STOP
    └── VOLVER
```

También aplica la paleta de JW Control:

```text
verde
blanco
negro
```

## Seguridad de arquitectura

Las acciones no se ejecutan dentro del callback gráfico.

Flujo:

```text
botón OK dentro de USER
→ la pantalla registra una solicitud de un byte
→ termina el callback y libera el bus SPI de la TFT
→ JWPLC_LogicRuntime_UI.update() procesa la acción desde loop()
→ el siguiente refresh actualiza únicamente los campos modificados
```

Esto evita usar FRAM mientras la TFT mantiene adquirido el bus SPI compartido.

## Ejemplo

```text
JWPLC_LogicRuntime_UI_Home
```

## Condición inicial esperada

Con la FRAM original sin formato y sin programa cargado:

```text
Runtime: READY
Programa: SIN PROGRAMA
ID / Gen: SIN IDENTIDAD
Bloques: 0
Boot: NO EVAL
Retentivos: RET -
Error: NONE
```

## Prueba visual

1. Iniciar en `IDLE`.
2. Confirmar que `RUN` y `ERR` permanecen apagados.
3. Entrar a `USER` con cualquier botón.
4. Verificar que el color de acento y la selección sean verdes.
5. Pulsar `OK` sobre `PROGRAMA`.
6. Confirmar la pantalla:

```text
PROGRAMA                              READY

IDENTIDAD
Nombre:   SIN PROGRAMA
ID / Gen: SIN IDENTIDAD
Bloques:  0 | Retentivos 0

ESTADO
Persist:  NO EVAL | RET -
Error:    NONE

[PREPARAR] [RUN] [STOP] [VOLVER]
```

7. Mover el selector varias vueltas.
8. Confirmar que solo se redibujen el botón anterior y el nuevo.
9. Mantener la pantalla quieta 20 segundos y comprobar ausencia de parpadeo.

## Prueba funcional segura sin programa

### PREPARAR

Pulsar `OK` en `PREPARAR`.

Resultado esperado con FRAM sin formato:

```text
Boot: SIN FORMATO
Error: STORED_PROGRAM_LOAD_FAILED
Mensaje: FRAM sin formato: no hay programa
Runtime: READY
```

La operación solo inspecciona y reconstruye en RAM cuando existe un programa válido. No formatea ni escribe FRAM.

### RUN

Pulsar `OK` en `RUN` sin programa cargado.

Resultado esperado:

```text
RUN rechazado
Error: PROGRAM_NOT_LOADED
Estado: READY
RUN de IDLE: apagado
```

### STOP

Pulsar `OK` en `STOP`.

Resultado esperado:

```text
Estado: STOPPED
Error: NONE
Mensaje: STOP aplicado; salidas apagadas
RUN de IDLE: apagado
Q0: apagadas
```

### VOLVER

Pulsar `OK` en `VOLVER`.

Resultado esperado:

```text
Retorno a HOME sin pasar por IDLE
HOME reconstruido una sola vez
```

`ESC` conserva su función global y regresa directamente a `IDLE` desde cualquier vista USER.

## Prueba de retorno y reconstrucción

1. Desde HOME abrir PROGRAMA.
2. Usar VOLVER.
3. Abrir PROGRAMA nuevamente.
4. Pulsar ESC y regresar a IDLE.
5. Entrar nuevamente a USER.

No deben quedar selecciones, mensajes o acciones pendientes de la visita anterior.

## Monitor serial esperado

```text
JWPLC Logic Runtime UI - Home y Programa
IDLE conserva monitor I/O y LEDs del package.
Pulsa cualquier boton para entrar a USER.

Storage: OK
Runtime: OK
Runtime UI: OK

Home: OK en PROGRAMA abre el control del runtime.
Programa: PREPARAR, RUN, STOP y VOLVER.
Las acciones se procesan fuera del callback grafico.
JWPLC_Display inicializado
```

## Datos a registrar

```text
Flash utilizada
RAM global utilizada
RAM restante
paleta verde visible: SI/NO
HOME -> PROGRAMA: PASS/FAIL
PREPARAR sin formato: PASS/FAIL
RUN sin programa: PASS/FAIL
STOP y Q0 apagadas: PASS/FAIL
VOLVER -> HOME: PASS/FAIL
ESC -> IDLE: PASS/FAIL
parpadeo en PROGRAMA: SI/NO
```

## Criterio de aprobación

```text
COMPILA
PALETA VERDE APLICADA
HOME -> PROGRAMA OPERATIVO
ACCIONES PROCESADAS FUERA DEL CALLBACK TFT
PREPARAR NO ESCRIBE FRAM
RUN SIN PROGRAMA SE RECHAZA DE FORMA SEGURA
STOP APAGA SALIDAS
VOLVER REGRESA A HOME
ESC REGRESA A IDLE
SIN PARPADEO
```