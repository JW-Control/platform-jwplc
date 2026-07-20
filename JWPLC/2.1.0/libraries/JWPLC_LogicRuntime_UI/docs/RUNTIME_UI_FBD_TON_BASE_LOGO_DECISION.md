# Decisión — cambio de BASE del TON tipo LOGO!

## Estado

```text
IMPLEMENTADA / PENDIENTE DE VALIDACIÓN FÍSICA
```

## Decisión

El editor TON de `RuntimeUIFBDMapUnified` no convierte la duración absoluta ni bloquea el cambio de base.

Cambiar `BASE` conserva los dos campos numéricos y modifica su interpretación:

```text
02:00s -> 02:00m -> 02:00h -> 02:00s
```

Correspondencia visual:

```text
s: SEG  / CENT
m: MIN  / SEG
h: HORA / MIN
```

## Rango del segundo campo

En base `s`, el segundo campo admite `00..99` centésimas.

En bases `m` y `h`, el segundo campo admite `00..59`. Si al cambiar desde segundos contiene `60..99`, se ajusta automáticamente a `59` sin bloquear:

```text
02:75s -> 02:59m
```

No se muestra `BASE NO EXACTA`.

## Aplicación

Al guardar:

1. se calcula el nuevo tiempo según la base seleccionada;
2. se actualizan `parameter` y `resource` en la sesión RAM;
3. se valida el programa completo;
4. se recarga el motor preservando su estado RUN;
5. el estado interno del TON y `Ta` reinician por la recarga.

No se escribe FRAM ni se conmutan salidas físicas desde la UI.

## Casos de prueba

- [ ] `02:00h -> 02:00s` sin bloqueo.
- [ ] `02:00s -> 02:00m` sin conversión a duración equivalente.
- [ ] `02:75s -> 02:59m` con ajuste automático.
- [ ] `02:59m -> 02:59h` sin cambio numérico.
- [ ] Nunca aparece `BASE NO EXACTA`.
- [ ] Guardar aplica el nuevo tiempo.
- [ ] `Ta` reinicia después de guardar.
- [ ] ESC conserva el tiempo original.
- [ ] El pie `OK GUARDAR / ESC CANCELAR` no parpadea durante cambios normales.

## Alcance

Este cambio no modifica:

```text
LogicV2BlockRecord
LogicV2InputLink
formato de 12 bytes
scan general del motor
sesión RAM transaccional
FRAM
salidas físicas
```
