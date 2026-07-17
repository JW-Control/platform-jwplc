# Validación física — editor FBD v0.5.1

## Objetivo

Validar el manejo jerárquico del editor gráfico y confirmar que `ESC` retrocede un solo nivel.

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
UI FBD v0.5.1: LISTA
```

## Controles definitivos de esta etapa

### MAPA FBD

```text
LEFT / RIGHT → navegar entre fuentes y consumidores
UP / DOWN    → navegar verticalmente
OK           → abrir DETALLE
ESC          → salir de USER
```

### DETALLE

```text
UP / DOWN → seleccionar entrada
OK        → abrir EDITAR IN
ESC       → volver a MAPA FBD
```

`LEFT` y `RIGHT` no deben abrir ni cerrar pantallas desde DETALLE.

### EDITAR IN

```text
LEFT / RIGHT → seleccionar FUENTE o LÓGICA
UP / DOWN    → cambiar el valor del campo seleccionado
OK           → validar y aplicar
ESC          → cancelar y volver a DETALLE
```

## Prueba 1 — avance con OK

1. Entrar a USER.
2. Seleccionar `B04 AND`.
3. Pulsar `OK` y confirmar que abre DETALLE.
4. Seleccionar `IN3/4` con `UP/DOWN`.
5. Pulsar `OK` y confirmar que abre EDITAR IN.

Resultado esperado:

```text
OK siempre avanza un nivel.
RIGHT ya no es necesario para entrar al editor.
```

## Prueba 2 — selección de campo

1. En EDITAR IN, pulsar `LEFT`.
2. Confirmar que el marco amarillo queda en `FUENTE`.
3. Pulsar `RIGHT`.
4. Confirmar que el marco amarillo queda en `LÓGICA`.

Resultado esperado:

```text
LEFT  → FUENTE
RIGHT → LÓGICA
```

## Prueba 3 — cambio con UP/DOWN

### FUENTE

1. Seleccionar `FUENTE`.
2. Pulsar `UP` y `DOWN`.
3. Confirmar que se recorren `X`, `HI`, `LO` y los bloques anteriores permitidos.

### LÓGICA

1. Seleccionar `LÓGICA`.
2. Pulsar `UP` o `DOWN`.
3. Confirmar alternancia:

```text
NORMAL ↔ NEGADA
```

## Prueba 4 — retroceso jerárquico con ESC

Partir desde EDITAR IN:

```text
Primer ESC  → DETALLE
Segundo ESC → MAPA FBD
Tercer ESC  → IDLE / pantalla principal
```

Resultado esperado:

- ningún `ESC` salta dos niveles;
- desde EDITAR IN no se abandona USER;
- desde DETALLE no se abandona USER;
- solo MAPA FBD conserva `IDLE_RETURN_ESC_ONLY`.

## Prueba 5 — aplicar

1. Cambiar una fuente o negación.
2. Pulsar `OK`.
3. Confirmar `APLICANDO CAMBIOS...`.
4. Confirmar retorno a DETALLE.
5. Verificar que la conexión y el valor lógico cambiaron.
6. Pulsar `ESC` una vez.
7. Confirmar retorno a MAPA FBD, no a IDLE.

## Prueba 6 — cancelar

1. Modificar temporalmente FUENTE o LÓGICA.
2. Pulsar `ESC`.
3. Confirmar retorno a DETALLE.
4. Confirmar que el programa activo no cambió.

## Prueba 7 — pulsos pendientes

En cada transición, mantener brevemente el botón usado y luego soltarlo:

- MAPA → DETALLE con `OK`;
- DETALLE → EDITAR IN con `OK`;
- EDITAR IN → DETALLE con `ESC`;
- DETALLE → MAPA con `ESC`;
- aplicar con `OK`.

Resultado esperado:

```text
No hay navegación automática posterior.
No se modifica otro campo por repetición residual.
No se salta una pantalla adicional.
```

## Criterio de aprobación

```text
Compila y sube desde Arduino IDE.
UI FBD v0.5.1 aparece en Serial.
OK avanza por MAPA → DETALLE → EDITAR IN.
LEFT/RIGHT selecciona FUENTE/LÓGICA.
UP/DOWN modifica el campo seleccionado.
ESC retrocede exactamente un nivel.
Solo ESC desde MAPA FBD sale de USER.
Aplicar modifica el programa RAM.
Cancelar conserva el programa activo.
No se escribe FRAM.
No se conmutan salidas físicas.
```
