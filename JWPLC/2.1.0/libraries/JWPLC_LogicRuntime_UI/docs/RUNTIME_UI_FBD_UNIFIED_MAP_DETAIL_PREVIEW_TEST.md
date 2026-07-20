# Prueba física — Unified MAPA + DETALLE preview

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN ARDUINO Y VALIDACIÓN FÍSICA
```

## Objetivo

Validar la primera fase funcional de `RuntimeUIFBDMapUnified`:

```text
MAPA FBD
DETALLE
navegación
cabecera de dos filas
valores vivos
T/Ta
transición MAPA <-> DETALLE
```

Esta implementación no hereda ni llama a:

```text
RuntimeUIFBDMapV4
RuntimeUIFBDMapV5
RuntimeUIFBDMapV7
RuntimeUIFBDMapV13
RuntimeUIFBDMapV14
RuntimeUIFBDMapActiveRenderer
```

## Ejemplo

```text
JWPLC_LogicRuntime_UI_FBD_Unified_Map_Detail_RAM
```

El ejemplo usa:

```cpp
JWPLC_LogicRuntime_UI.beginUnifiedPreview(engine);
```

El `begin(engine)` normal conserva el renderer anterior como fallback.

## Alcance deliberado

En este preview:

- `OK` desde MAPA abre DETALLE.
- `ESC` desde DETALLE vuelve a MAPA.
- `LEFT/RIGHT/UP/DOWN` navegan en MAPA.
- `LEFT/RIGHT` alternan `Trg / PARAM T` en el TON.
- `UP/DOWN` recorren entradas en bloques con varias entradas.
- `OK` dentro de DETALLE no abre todavía `EDITAR IN` ni `EDITAR T`.
- No existe todavía el nodo `+` consolidado.
- No se escribe FRAM.
- No se conmutan salidas físicas.

## Política de render

- Ninguna transición interna usa `clearScreen()` ni `fillScreen()`.
- El contenido destino sustituye una vez el interior compartido.
- La cabecera se dibuja después del contenido final.
- Título, contexto y estado tienen cachés independientes.
- `RUN` no debe limpiarse ni repintarse al cambiar de vista si no cambia el estado.
- `Bxx TIPO` no debe limpiarse al cambiar de vista si el bloque seleccionado no cambia.
- El callback sigue siendo incondicional para no bloquear botones.

## Compilación

- [ ] Compila desde Arduino IDE.
- [ ] Compila con Arduino CLI `--clean`.
- [ ] Aparecen en el log:
  - [ ] `RuntimeUIFBDMapUnified.cpp`
  - [ ] `RuntimeUIFBDMapUnifiedMap.cpp`
  - [ ] `RuntimeUIFBDMapUnifiedDetail.cpp`
- [ ] No hay símbolos duplicados con las clases Vx.
- [ ] La librería seleccionada corresponde al package local correcto.

## IDLE -> MAPA

- [ ] Un botón despierta USER normalmente.
- [ ] Aparece el mapa completo de 11 bloques.
- [ ] La cabecera muestra `MAPA FBD`.
- [ ] Fila 1 muestra `Bxx TIPO`.
- [ ] Fila 2 muestra `ON/OFF`.
- [ ] RUN aparece correctamente.
- [ ] No hay traslapes.

## Navegación en MAPA

- [ ] LEFT selecciona una fuente válida.
- [ ] RIGHT selecciona un consumidor válido.
- [ ] UP/DOWN navegan por proximidad vertical.
- [ ] El bloque anterior pierde el marco amarillo.
- [ ] El bloque nuevo recibe el marco amarillo.
- [ ] La cabecera sigue la selección.
- [ ] La ventana horizontal conserva edge hints.
- [ ] Los botones responden aunque `Trg` esté apagado.

## MAPA -> DETALLE

- [ ] OK abre DETALLE.
- [ ] Existe un único cambio visible.
- [ ] No aparece barrido negro diagonal.
- [ ] No aparece un frame de cabecera histórica.
- [ ] `Bxx TIPO` permanece en la misma posición.
- [ ] RUN permanece estable y no se limpia.
- [ ] La región interior muestra fuente, cable y bloque final directamente.

## DETALLE

Para B07 TON:

- [ ] Cabecera fila 1: `B07 TON`.
- [ ] Cabecera fila 2 inicial: `Trg`.
- [ ] RIGHT cambia a `PARAM T`.
- [ ] LEFT vuelve a `Trg`.
- [ ] El marco amarillo de T aparece completo al seleccionar parámetro.
- [ ] `T` conserva `02:00s`.
- [ ] `Ta` usa siempre sufijo `s`.
- [ ] `Ta` estático no parpadea.
- [ ] Durante temporización solo cambia el valor visible de `Ta`.
- [ ] Los pines, cables y fuentes siguen el estado lógico.

Para AND/SR:

- [ ] UP/DOWN recorren entradas.
- [ ] La fila 2 muestra el rol o `INx/n` correcto.
- [ ] El marco amarillo cambia de fuente sin limpiar toda la pantalla.

## DETALLE -> MAPA

- [ ] ESC vuelve al MAPA.
- [ ] Existe un único cambio visible.
- [ ] No aparece barrido negro.
- [ ] `Bxx TIPO` permanece estable si el bloque no cambia.
- [ ] RUN permanece estable.
- [ ] El mapa conserva el bloque seleccionado.

## Fuera de alcance

Al pulsar OK dentro de DETALLE:

```text
No debe abrirse ningún editor todavía.
```

Esto no es una falla de esta fase. `EDITAR IN` se migrará en U2 y `EDITAR T` en U3.

## Regresión del fallback

Después del preview puede cargarse nuevamente:

```text
JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

Ese ejemplo usa `begin(engine)` normal y debe continuar empleando el renderer anterior hasta completar la consolidación.

## Resultado

```text
Compilación:
IDLE -> MAPA:
Navegación MAPA:
MAPA -> DETALLE:
DETALLE:
DETALLE -> MAPA:
Botonera con Trg OFF:
T/Ta:
Observaciones visuales:
Decisión: APROBADO U1 / REQUIERE CORRECCIÓN
```
