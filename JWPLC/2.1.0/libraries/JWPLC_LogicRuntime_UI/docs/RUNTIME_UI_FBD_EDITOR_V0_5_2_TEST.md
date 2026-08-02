# Prueba física — editor FBD v0.5.2: entradas y parámetros TON

## Objetivo

Validar el patrón definitivo de navegación dentro de `DETALLE`:

```text
LEFT / RIGHT → cambiar entre ENTRADAS y PARÁMETROS
UP / DOWN    → seleccionar entrada o parámetro
OK           → editar el elemento seleccionado
ESC          → volver a MAPA FBD
```

La primera aplicación del patrón se implementa sobre `TON`:

- `Trg` como entrada de señal editable;
- `T` como parámetro editable;
- `Ta` como diagnóstico de solo lectura;
- `Q` representada gráficamente mediante el estado del bloque y su salida.

## Ejemplo

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

## Serial esperado

```text
Motor v2: RUNNING
UI FBD v0.5.2: ENTRADAS + PARAMETROS TON
```

## Controles

### MAPA FBD

```text
LEFT / RIGHT → navegar entre fuentes y consumidores
UP / DOWN    → navegar verticalmente
OK           → abrir DETALLE
ESC          → salir de USER
```

### DETALLE

```text
LEFT          → foco en ENTRADAS
RIGHT         → foco en PARÁMETROS cuando existen
UP / DOWN     → recorrer elementos del grupo activo
OK            → abrir el editor del elemento seleccionado
ESC           → volver a MAPA FBD
```

En bloques sin parámetros, `LEFT/RIGHT` no crea un grupo vacío.

### EDITAR IN

```text
LEFT / RIGHT → seleccionar FUENTE o LÓGICA
UP / DOWN    → modificar el campo seleccionado
OK           → aplicar
ESC          → cancelar y volver a DETALLE
```

### EDITAR T

```text
LEFT / RIGHT → seleccionar VALOR o UNIDAD
UP / DOWN    → modificar el campo seleccionado
OK           → aplicar
ESC          → cancelar y volver a DETALLE
```

Unidades iniciales:

```text
ms
s
min
h
```

El valor almacenado por el motor continúa expresándose en milisegundos.

## Prueba 1 — foco Entrada/Parámetro

1. Entrar a `DETALLE` del bloque `B07 TON`.
2. Confirmar que el foco inicial está en `Trg`.
3. Pulsar `RIGHT`.
4. Confirmar que el marco amarillo pasa a `T`.
5. Pulsar `LEFT`.
6. Confirmar que el foco vuelve a `Trg`.

Resultado esperado:

```text
LEFT/RIGHT solo mueve el foco.
No abre ni cierra pantallas.
```

## Prueba 2 — edición de Trg

1. Con el foco en `Trg`, pulsar `OK`.
2. Confirmar apertura de `EDITAR IN`.
3. Modificar `FUENTE` o `LÓGICA`.
4. Pulsar `ESC` y confirmar que cancela.
5. Repetir y pulsar `OK` para aplicar.

Resultado esperado:

- el flujo validado en v0.5.1 se conserva;
- el programa activo solo cambia al confirmar;
- se vuelve a `DETALLE`.

## Prueba 3 — edición de T

1. Con el foco en `T`, pulsar `OK`.
2. Confirmar apertura de `EDITAR T`.
3. Seleccionar `VALOR` con `LEFT`.
4. Cambiar el valor con `UP/DOWN`.
5. Seleccionar `UNIDAD` con `RIGHT`.
6. Recorrer `ms/s/min/h` con `UP/DOWN`.
7. Pulsar `OK`.

Resultado esperado:

- aparece `APLICANDO CAMBIOS...`;
- el motor se recarga transaccionalmente en RAM;
- el TON reinicia su estado temporal;
- se vuelve a `DETALLE` con foco en `T`;
- no se escribe FRAM.

## Prueba 4 — cancelar T

1. Abrir `EDITAR T`.
2. Cambiar valor y unidad.
3. Pulsar `ESC`.

Resultado esperado:

```text
El parámetro activo no cambia.
Se vuelve a DETALLE con foco en T.
```

## Prueba 5 — visualización T y Ta

Observar `B07 TON` durante el ciclo automático.

Resultado esperado:

- `T` muestra el tiempo configurado;
- `Ta` avanza durante la temporización;
- `Ta` se actualiza sin redibujar toda la pantalla;
- al finalizar, `Ta` queda saturado en `T`;
- no aparece una línea textual `Q = 0/1`.

La salida del TON debe seguir viéndose mediante:

- borde y símbolo del bloque;
- terminal derecho;
- cable de salida;
- color verde cuando está activa.

## Prueba 6 — jerarquía ESC

Desde `EDITAR T`:

```text
Primer ESC  → DETALLE
Segundo ESC → MAPA FBD
Tercer ESC  → IDLE
```

Desde `EDITAR IN` debe mantenerse el mismo patrón.

## Prueba 7 — pulsos pendientes

Mantener brevemente el botón usado en cada transición:

- `MAPA → DETALLE`;
- `DETALLE → EDITAR IN`;
- `DETALLE → EDITAR T`;
- `EDITAR IN → DETALLE`;
- `EDITAR T → DETALLE`;
- `DETALLE → MAPA`.

Resultado esperado:

```text
No se salta una pantalla.
No se modifica otro campo por repetición residual.
No se abandona USER desde niveles internos.
```

## Criterio de aprobación

```text
Compila y sube desde Arduino IDE.
UI FBD v0.5.2 aparece en Serial.
LEFT/RIGHT cambia entre entradas y parámetros.
UP/DOWN recorre el grupo activo.
OK edita Trg o T según el foco.
T se aplica transaccionalmente en RAM.
Ta se actualiza durante la temporización.
Q no se duplica como texto.
ESC retrocede exactamente un nivel.
No se escribe FRAM.
No se conmutan salidas físicas.
```
