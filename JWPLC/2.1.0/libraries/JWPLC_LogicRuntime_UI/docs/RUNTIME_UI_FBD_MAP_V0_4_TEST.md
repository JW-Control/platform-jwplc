# Validación física — mapa FBD v0.4.0

## Objetivo

Validar el regreso a `JWPLC_LogicRuntime_UI` usando el backend RAM v2 ya aprobado.

La vista reemplaza el modelo centrado por bloque de v0.3.1 por un mapa FBD estable:

```text
posición fija por bloque
viewport desplazable
navegación por conexiones
puertos variables
negación individual por pin
valores lógicos en vivo
estado temporal TON en detalle
```

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

## Programa de demostración

```text
B00  I0.0 simulada
B01  I0.1 simulada
B02  I0.2 simulada
B03  I0.3 simulada
B04  AND4: B00, B01, !B02, B03
B05  OR2: B02, LO
B06  SET/RESET: S=B04, R=B05
B07  TON B06, 2000 ms
B08  NOT B07
B09  Q0.0 lógica <- B07
B10  Q0.1 lógica <- B08
```

Capacidad usada:

```text
11 bloques
12 enlaces
```

## Secuencia automática

El sketch usa entradas simuladas y repite un ciclo de 9 segundos:

```text
0..2 s  estado inicial
2..3 s  AND4=TRUE y SET
3..6 s  SR conserva estado y TON termina
6..7 s  I0.2=TRUE y RESET
7..9 s  espera apagada
```

## Controles

```text
LEFT   ir a una fuente conectada
RIGHT  ir a un consumidor conectado
UP     bloque geográfico superior
DOWN   bloque geográfico inferior
OK     abrir/cerrar detalle
ESC    volver a IDLE
```

La selección usa borde amarillo. El verde queda reservado para señales `TRUE`, de modo que selección y estado lógico no se confunden.

## Comportamiento visual esperado

### Mapa

- Los bloques conservan su posición al cambiar la selección.
- El cursor se desplaza entre nodos; el mapa no se reconstruye como páginas B00, B01, B02.
- El viewport se mueve únicamente cuando el nodo seleccionado alcanza los márgenes.
- Los cables se dibujan antes que los nodos.
- Cable verde: fuente lógica activa.
- Cable gris: fuente lógica inactiva.
- Borde amarillo: bloque seleccionado.
- Burbuja blanca: entrada negada.
- Los bloques muestran nombre corto, recurso o cantidad de entradas.

### Detalle

Debe mostrar:

```text
bloque
tipo
valor
cantidad de entradas
recurso
parámetro
conexiones
```

Para `B07 TON` también debe mostrar:

```text
IDLE / CONTANDO / LISTO
tiempo transcurrido
tiempo restante
```

## Aislamiento

```text
No abre ni escribe FRAM
No modifica codec v1
No usa almacenamiento A/B
No conmuta salidas Q0 físicas
No habilita edición gráfica
No guarda cambios desde la TFT
```

`B09` y `B10` son salidas lógicas internas del motor experimental.

El autoload normal del JWPLC Basic continúa activo; no se elimina ningún periférico para esta prueba.

## Monitor Serial esperado

Al arrancar:

```text
JWPLC Logic Runtime UI - mapa FBD v2 en RAM
11 bloques, 12 enlaces, AND4 con pin negado y TON 2 s.
No escribe FRAM ni conmuta salidas Q0 fisicas.
Motor v2: RUNNING
UI FBD v0.4.0: LISTA
Pulse cualquier boton para entrar a USER.
```

Durante la ejecución se imprimen únicamente cambios de fase y los estados lógicos de SR, TON, Q0.0 y Q0.1.

## Datos a registrar

```text
Flash usada
RAM global
RAM restante
estado Serial
foto del mapa inicial
foto del viewport desplazado
foto del detalle de B07 TON contando
foto del detalle de B07 TON listo
```

## Criterio de aprobación

```text
COMPILACIÓN: OK
MOTOR V2: RUNNING
MAPA ESTABLE: OK
NAVEGACIÓN POR CONEXIONES: OK
PUERTOS VARIABLES: OK
NEGACIÓN POR PIN: VISIBLE
TON EN VIVO: OK
SIN CONMUTACIÓN Q0 FÍSICA: OK
```

Esta fase es de inspección y navegación. La edición gráfica y la persistencia v2 permanecen fuera de alcance.
