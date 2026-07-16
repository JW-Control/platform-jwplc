# Validación física — mapa FBD v0.4.5

## Objetivo

Validar la compactación final del mapa FBD y la primera vista gráfica de detalle sobre el motor RAM v2.

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

El monitor Serial debe indicar:

```text
Motor v2: RUNNING
UI FBD v0.4.5: LISTA
```

## Cambios esperados en el mapa

### Área útil

El footer de ayuda desaparece. El panel gráfico ocupa casi toda la TFT:

```text
y inicial: 27
y final:   168
```

### Encabezado

La información del bloque seleccionado comienza en `x=112`, ligeramente más separada del título `MAPA FBD`.

Formato esperado:

```text
B03 4/11 0,38
```

### Bloques compactos

Los bloques miden `48 x 30 px` y usan dos líneas.

Compuerta:

```text
B04
AND
```

Entrada física:

```text
B00
I0.0
```

Salida lógica:

```text
B09
Q0.0
```

Ya no deben aparecer `4 IN`, `2 IN` o `1 IN` en el mapa.

### Columnas laterales

Las referencias izquierda y derecha usan el mismo formato de dos líneas y el mismo tamaño:

```text
<B04     B07>
 AND      TON
```

Solo se muestra la columna inmediatamente anterior o siguiente. Las referencias de ambos lados pueden aparecer simultáneamente cuando el viewport queda entre columnas.

Una referencia lateral no debe cubrir un bloque completo del área principal.

### Cableado y señales

- cable inactivo: gris;
- cable activo: verde;
- selección: amarillo;
- burbuja de negación: gris;
- `S`, `R` y `T`: dentro del gutter del bloque.

## Vista gráfica de detalle

Pulse `OK` sobre un bloque.

La pantalla debe mostrar:

- fuentes conectadas en tarjetas a la izquierda;
- cables ortogonales hacia el bloque;
- bloque ampliado a la derecha;
- símbolo funcional grande;
- estado de las señales mediante gris o verde.

Símbolos iniciales:

```text
AND   &
OR    >=1
XOR   =1
NOT   1
SR    SR
TON   TON
```

Para TON también se muestra el tiempo configurado. Para entradas y salidas se muestra el recurso `I0.x` o `Q0.x`.

### Controles del detalle

```text
UP/DOWN  seleccionar una entrada
LEFT     saltar al bloque fuente seleccionado y volver al mapa
OK       volver al mapa
ESC      retorno general a IDLE por JWPLC_Display
```

Con más de cuatro entradas, `UP/DOWN` cambia automáticamente entre grupos de cuatro.

## Alcance y seguridad

```text
Vista de detalle: solo lectura
Edición de enlaces: no implementada todavía
Escritura FRAM: no
Conmutación de Q físicas: no
Almacenamiento A/B: no
Autoload normal de periféricos: sin cambios
```

## Casos a fotografiar

1. Vista inicial con cuatro entradas y bloques compactos.
2. Vista intermedia con referencia izquierda y derecha simultáneas.
3. Vista final con B07, B08, B09 y B10.
4. Detalle gráfico de B04 AND.
5. Detalle gráfico de B05 OR.
6. Detalle gráfico de B06 SR.
7. Detalle gráfico de B07 TON.
8. Cambio de entrada activa sin parpadeo fuerte.

## Datos a registrar

```text
Flash usada
RAM global
RAM restante
salida Serial inicial
resultado de navegación
resultado visual del mapa
resultado visual del detalle
parpadeo observado
```

## Criterio de aprobación

```text
FOOTER ELIMINADO: OK
MAPA COMPACTO: OK
CUATRO FILAS VISIBLES: OK
HINT IZQUIERDO SIMÉTRICO: OK
HINT DERECHO SIMÉTRICO: OK
SIN SUPERPOSICIÓN: OK
DETALLE GRÁFICO: OK
NAVEGACIÓN DE DETALLE: OK
SEÑALES EN VIVO: OK
PARPADEO CONTROLADO: OK
```
