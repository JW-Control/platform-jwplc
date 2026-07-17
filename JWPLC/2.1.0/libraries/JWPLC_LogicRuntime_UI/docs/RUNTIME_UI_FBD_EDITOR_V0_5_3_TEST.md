# Prueba física — editor FBD v0.5.3

## Objetivo

Validar las dos correcciones posteriores a v0.5.2:

1. repeat acelerado al mantener `UP/DOWN` sobre `VALOR`;
2. actualización continua de `Ta LECTURA` dentro de `EDITAR T`.

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
UI FBD v0.5.3: REPEAT T + Ta VIVO
```

## Prueba 1 — pulsación corta

1. Entrar a `B07 TON`.
2. Mover el foco a `T` con `RIGHT`.
3. Pulsar `OK` para abrir `EDITAR T`.
4. Dejar seleccionado `VALOR`.
5. Pulsar brevemente `UP`.
6. Pulsar brevemente `DOWN`.

Resultado esperado:

```text
UP corto   → aumenta una unidad
DOWN corto → reduce una unidad
```

No debe existir salto circular en los límites.

## Prueba 2 — pulso mantenido

1. Mantener `UP` durante al menos dos segundos.
2. Observar el valor mostrado.
3. Repetir manteniendo `DOWN`.

Resultado esperado:

- el primer cambio es inmediato;
- tras el retardo de repeat, el valor continúa cambiando sin soltar el botón;
- la frecuencia aumenta según el perfil global de `JW_MatrixButtons`;
- no se generan saltos al cambiar de pantalla;
- al soltar el botón, el valor deja de cambiar.

La UI debe consumir `PRESS` y `EV_REPEAT` mediante `applyAxis()`; no debe implementar un segundo sistema de aceleración por tiempo.

## Prueba 3 — unidad sin repeat agresivo

1. Seleccionar `UNIDAD` con `RIGHT`.
2. Pulsar `UP/DOWN`.
3. Confirmar recorrido entre:

```text
ms
s
min
h
```

El cambio de unidad conserva el manejo discreto existente.

## Prueba 4 — Ta vivo sin tocar botones

1. Permanecer en `EDITAR T` sin tocar la botonera.
2. Esperar el inicio del TON durante el ciclo automático.
3. Observar `Ta LECTURA`.

Resultado esperado:

- `Ta` avanza aproximadamente cada 100 ms;
- no requiere cambiar `VALOR` ni `UNIDAD` para refrescarse;
- solo se redibuja el campo numérico de `Ta`;
- no hay parpadeo visible del panel completo;
- al finalizar, `Ta` queda saturado en `T ACTUAL`.

## Prueba 5 — borrador frente a valor aplicado

1. Cambiar `VALOR` o `UNIDAD` sin confirmar.
2. Observar `T ACTUAL` y `Ta LECTURA`.
3. Pulsar `ESC`.

Resultado esperado:

- `T ACTUAL` sigue mostrando el parámetro aplicado al motor;
- `Ta LECTURA` sigue perteneciendo al TON activo;
- el borrador visible en `VALOR/UNIDAD` no modifica el motor hasta pulsar `OK`;
- `ESC` cancela y conserva el valor anterior.

## Prueba 6 — aplicar

1. Abrir `EDITAR T`.
2. Ajustar `VALOR` usando pulso mantenido.
3. Seleccionar la unidad deseada.
4. Pulsar `OK`.

Resultado esperado:

- aparece `APLICANDO CAMBIOS...`;
- se valida y recarga el programa en RAM;
- se reinicia el estado temporal del TON;
- se vuelve a `DETALLE` con foco en `T`;
- no se escribe FRAM;
- no se conmuta ninguna salida física.

## Criterio de aprobación

```text
Compila y sube desde Arduino IDE.
UI FBD v0.5.3 aparece en Serial.
UP/DOWN corto cambia una unidad.
UP/DOWN mantenido genera repeat continuo y acelerado.
Ta se actualiza sin interacción del usuario.
El refresco de Ta no reconstruye toda la pantalla.
ESC cancela el borrador.
OK aplica transaccionalmente en RAM.
No se escribe FRAM.
No se conmutan salidas físicas.
```
