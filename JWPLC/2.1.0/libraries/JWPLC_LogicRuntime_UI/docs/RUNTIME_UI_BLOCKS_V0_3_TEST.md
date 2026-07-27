# Prueba física — Runtime UI Bloques v0.3

## Estado

```text
IMPLEMENTACIÓN PREPARADA
COMPILACIÓN Y VALIDACIÓN EN TFT PENDIENTES
```

## Alcance

Esta etapa incorpora:

```text
HOME
└── BLOQUES
    ├── lista desplazable
    ├── valores booleanos en vivo
    ├── detalle de definición
    └── VOLVER a HOME
```

La vista es de solo lectura. No edita, guarda, formatea ni modifica el programa.

## API de lectura añadida

`JWPLC_LogicRuntime` expone vistas `const` del programa que ya vive dentro del motor:

```cpp
runtime.program();
runtime.blockCount();
runtime.blockDefinition(index);
runtime.blockValue(index);
```

Los punteros pertenecen al runtime y no deben modificarse ni conservarse después de cargar o descargar otro programa.

## Ejemplo

```text
JWPLC_LogicRuntime_UI_Blocks
```

El ejemplo carga en RAM siete bloques:

```text
0  ENTRADA    I0.0
1  ENTRADA    I0.1
2  NOT        A=0
3  AND        A=0 B=1
4  OR         A=2 B=3
5  SET/RESET  A=3 B=1  RETENTIVO
6  TON        A=4      2000 ms
```

No contiene bloques `DigitalOutput`, por lo que Q0 permanece apagado.

## Seguridad

```text
FRAM: solo inspección inicial
Formato: no
Escrituras persistentes: no
Programa: únicamente RAM
Salidas digitales: ninguna
Runtime: RUN para actualizar valores de los bloques
```

La prueba no debe alterar el contenido actual de la FRAM.

## Inicio esperado

Monitor serial:

```text
JWPLC Logic Runtime UI - lista y detalle de bloques
Carga un programa RAM de 7 bloques sin salidas digitales.
No formatea ni escribe la FRAM.

Storage: OK
Runtime: OK
Programa RAM: OK
RUN: OK
Runtime UI: OK
```

En HOME debe verse:

```text
Estado: RUNNING
Programa: DEMO UI BLOQUES
ID / Gen: SIN IDENTIDAD
Bloques: 7
```

La falta de identidad persistente es correcta porque el programa de prueba vive solo en RAM.

## Lista de bloques

1. Entrar a USER.
2. Seleccionar `BLOQUES`.
3. Pulsar `OK`.

Debe aparecer una lista de cinco filas visibles con columnas aproximadas:

```text
#  TIPO      A    B    RECURSO   V
```

Controles:

```text
UP / DOWN     seleccionar bloque y desplazar la ventana
LEFT / RIGHT  alternar DETALLE / VOLVER
OK            ejecutar la acción seleccionada
ESC           volver directamente a IDLE
```

Al bajar desde el bloque 4 deben aparecer los bloques 5 y 6 sin reconstruir innecesariamente el resto de la interfaz.

## Valores en vivo

La columna `V` se evalúa visualmente cada 100 ms, pero solo se redibuja una fila cuando cambia su valor.

Comprobar:

1. sin tocar entradas, el bloque NOT y el TON evolucionan según la lógica;
2. cambiar I0.0 e I0.1 modifica las filas relacionadas;
3. no parpadean las filas cuyos valores permanecen estables;
4. la navegación sigue respondiendo mientras el runtime ejecuta scan.

## Detalle

Seleccionar un bloque, mantener `DETALLE` activo y pulsar `OK`.

Debe mostrarse:

```text
Bloque:    posición dentro del programa
Tipo:      ENTRADA / SALIDA / NOT / AND / OR / SET/RESET / TON
Fuente A
Fuente B
Recurso:   I0.x, Q0.x o información aplicable
Parámetro: tiempo TON cuando corresponda
Flags:     RETENTIVO o NINGUNO
Valor:     TRUE o FALSE
```

En detalle:

```text
UP / LEFT      bloque anterior
DOWN / RIGHT   bloque siguiente
OK             regresar a la lista
ESC            volver a IDLE
```

El campo `Valor` debe actualizarse sin reconstruir los campos de definición.

## Retorno

Desde la lista:

1. usar LEFT/RIGHT para seleccionar `VOLVER`;
2. pulsar `OK`;
3. confirmar retorno a HOME sin pasar por IDLE;
4. abrir BLOQUES nuevamente y comprobar reconstrucción rápida.

## Datos a registrar

```text
Flash utilizada
RAM global utilizada
RAM restante
Programa RAM: OK/FAIL
RUN: OK/FAIL
HOME muestra nombre y 7 bloques: PASS/FAIL
HOME -> BLOQUES: PASS/FAIL
scroll hasta bloque 6: PASS/FAIL
detalle de ENTRADA: PASS/FAIL
detalle de SET/RESET retentivo: PASS/FAIL
detalle de TON 2000 ms: PASS/FAIL
valores en vivo: PASS/FAIL
VOLVER -> HOME: PASS/FAIL
ESC -> IDLE: PASS/FAIL
parpadeo: SI/NO
carga inicial lenta: SI/NO
Q0 apagadas: SI/NO
```

## Criterio de aprobación

```text
COMPILA
PROGRAMA RAM CARGADO
RUNTIME EN RUN
HOME IDENTIFICA PROGRAMA RAM
LISTA DE 7 BLOQUES OPERATIVA
SCROLL OPERATIVO
DETALLE OPERATIVO
VALORES EN VIVO SIN PARPADEO
VOLVER REGRESA A HOME
ESC REGRESA A IDLE
FRAM SIN ESCRITURAS
Q0 APAGADAS
```