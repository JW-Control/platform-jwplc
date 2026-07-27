# Regresión v0.6.1 — compuerta previa al callback USER

## Estado

```text
CORREGIDA EN CÓDIGO / PENDIENTE DE VALIDACIÓN FÍSICA
```

## Síntoma observado

En la pantalla `DETALLE`, con el TON y su entrada `Trg` inactivos, los botones no eran procesados. La navegación reaparecía únicamente cuando `Trg` cambiaba y el TON generaba una modificación lógica visible.

## Causa

Se añadió en `JWPLC_Display` una consulta previa al bloqueo SPI:

```cpp
jwplcUserDisplayRefreshNeededCallback(io, rtc)
```

La consulta pretendía evitar adquirir el bus TFT cuando no existían regiones visuales sucias. Sin embargo, `RuntimeUIFBDMap::refresh()` todavía combina dos responsabilidades:

1. consumir y procesar eventos de botonera;
2. actualizar regiones TFT.

Al omitir el callback completo por considerar la pantalla visualmente estática, también se omitía el procesamiento de entrada. Un cambio de `Trg` hacía que el predicado visual devolviera verdadero y permitía procesar posteriormente eventos de botón pendientes.

## Corrección inmediata

`RuntimeUIFBDMap::needsTftRefresh()` devuelve incondicionalmente `true`.

Esto restaura el flujo anterior:

```text
callback periódico
→ procesar botones
→ cachés deciden si hay que escribir píxeles
```

Se conservan:

- frecuencia adaptativa de 100 ms en MAPA/DETALLE;
- frecuencia de 40 ms en asistentes y editores;
- cachés regionales de T/Ta;
- transición por bandas DETALLE → EDITAR T;
- encabezado DETALLE de dos filas.

Solo queda desactivado el salto previo del callback completo.

## Regla arquitectónica

No volver a omitir el callback USER mientras entrada y render compartan `refresh()`.

La optimización SPI solo podrá reactivarse después de separar explícitamente:

```text
updateInputAndState()  // sin bus SPI
needsRender()          // consulta dirty-region
render()               // con bus TFT adquirido
```

## Prueba física obligatoria

Con `Trg` permanentemente inactivo:

- [ ] LEFT cambia correctamente entre entrada y parámetro.
- [ ] RIGHT cambia correctamente entre entrada y parámetro.
- [ ] UP/DOWN navegan cuando corresponda.
- [ ] OK abre `EDITAR T` desde `PARAM T`.
- [ ] ESC regresa al mapa.
- [ ] Ninguna acción requiere un pulso de `Trg`.

Repetir:

- [ ] mientras TON está temporizando;
- [ ] después de que TON complete;
- [ ] con TON nuevamente inactivo.

## Criterio de cierre

```text
Botonera con Trg inactivo: APROBADA / FALLA
Botonera durante TON:      APROBADA / FALLA
Botonera TON completado:   APROBADA / FALLA
Decisión:                  REGRESIÓN CERRADA / REQUIERE CORRECCIÓN
```
