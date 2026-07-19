# Prueba física — TON con base de tiempo tipo LOGO! v0.6.0 candidata

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

Esta revisión amplía la candidata v0.5.9. No debe marcarse como aprobada ni publicarse hasta completar la prueba.

## Objetivo

Reemplazar el editor entero `VALOR + UNIDAD` del TON por el modelo de base de tiempo utilizado por Siemens LOGO!:

| Base | Interpretación `__:__` | Máximo | Resolución |
|---|---|---:|---:|
| `s` | segundos : centésimas | `99:99s` | 10 ms |
| `m` | minutos : segundos | `99:59m` | 1 s |
| `h` | horas : minutos | `99:59h` | 1 min |

Ejemplo:

```text
04:10h = 4 horas + 10 minutos
```

## Decisión de almacenamiento

`LogicV2BlockRecord` conserva 12 bytes.

- `parameter`: tiempo efectivo en milisegundos, utilizado por el motor TON.
- `resource` del TON, bits `1..0`: base preferida para edición y presentación.
- `flags`: permanece en cero; no se consume porque queda reservado para retentividad y metadatos generales futuros.

Codificación:

```text
00 = s
01 = m
10 = h
11 = reservado; la UI lo interpreta defensivamente como s
```

Los bloques DI y DO continúan usando `resource` como recurso físico. La nueva interpretación solo aplica cuando `type == LogicV2BlockType::Ton`.

## Manejo por botonera

El editor muestra tres campos:

```text
SEG  <04> | CENT <10> | BASE <s>
MIN  <04> | SEG  <10> | BASE <m>
HORA <04> | MIN  <10> | BASE <h>
```

- `LEFT/RIGHT`: mueve el foco entre primer componente, segundo componente y base.
- `UP/DOWN`: modifica únicamente el campo seleccionado.
- Cambiar la base conserva ambos números.
- Al pasar de `s` a `m` o `h`, el segundo componente se limita a `59` si estaba entre `60` y `99`.
- `OK`: calcula milisegundos, guarda `parameter + resource` en el borrador y aplica transaccionalmente.
- `ESC`: cancela el borrador y conserva el TON original.

## Restricciones

- Tiempo mínimo editable: `00:02s` = 20 ms.
- No se escribe FRAM.
- No se conmutan salidas físicas.
- Cambiar/aplicar la base recarga el motor RAM y reinicia el tiempo transcurrido del TON.
- Un programa v2 anterior con `TON.resource == 0` se interpreta inicialmente con base `s`. Si el valor no cabe o no tiene resolución compatible, la UI elige una base representable para mostrarlo, sin modificarlo hasta pulsar `OK`.

## Archivos

```text
src/screens/RuntimeUIFBDMapV5.h
src/screens/RuntimeUIFBDMapV11.h
src/screens/RuntimeUIFBDMapV13.h
src/screens/RuntimeUIFBDMapV13.cpp
src/JWPLC_LogicRuntime_UI.h
```

## Compilación

- [ ] Compila desde checkout limpio.
- [ ] `RuntimeUIFBDMapV13.cpp` aparece en el log de compilación.
- [ ] `JWPLC_LogicRuntime_UI.h` instancia `RuntimeUIFBDMapV13`.
- [ ] No aparecen errores de acceso privado en V4/V5/V8.
- [ ] No aparecen ambigüedades en métodos virtuales de V11/V12/V13.
- [ ] `sizeof(LogicV2BlockRecord) == 12` continúa vigente.

## Creación de TON

- [ ] Crear `01:00s` y confirmar que equivale a 1 segundo.
- [ ] Crear `04:10h` y verificar que la pantalla principal conserva `04:10h`.
- [ ] Crear `03:25m` y verificar que la pantalla principal conserva `03:25m`.
- [ ] Intentar crear `00:00s`; debe mostrar `T MINIMO 00:02s`.
- [ ] Cancelar con `ESC`; no debe agregarse ningún bloque.

## Edición de TON existente

- [ ] Abrir un TON creado como `04:10h`; debe reabrir exactamente como `04:10h`.
- [ ] Cambiar solo la base de `04:10h` a `04:10m`; los números no deben recalcularse.
- [ ] Cambiar de `10:99s` a base `m`; el segundo componente debe quedar limitado a `59`.
- [ ] Guardar y volver a abrir; la base elegida debe conservarse.
- [ ] Cancelar con `ESC`; parámetro y base deben quedar sin cambios.
- [ ] `Ta LECTURA` debe mostrarse en la base actualmente seleccionada.

## Detalle TON

- [ ] El panel de detalle muestra `T __:__x` usando la base guardada.
- [ ] El panel muestra `Ta __:__x` con la misma base.
- [ ] La actualización regional no produce parpadeo general.
- [ ] El bloque y sus conexiones permanecen visibles.

## Regresión v0.5.9

- [ ] El nodo `+` compacto no reaparece.
- [ ] El bloque final no se traslapa.
- [ ] `ESC` sigue retornando un nivel dentro de pantallas anidadas.
- [ ] DI, NOT, AND2 y DO se crean igual que antes.
- [ ] El editor de fuentes no cambia.
- [ ] La lista de cuatro parámetros y el indicador `01/08` permanecen disponibles para bloques futuros.

## Resultado

```text
Compilación:
Creación TON:
Edición TON:
Detalle TON:
Regresión:
Observaciones:
Decisión: APROBADA / REQUIERE CORRECCIÓN
```
