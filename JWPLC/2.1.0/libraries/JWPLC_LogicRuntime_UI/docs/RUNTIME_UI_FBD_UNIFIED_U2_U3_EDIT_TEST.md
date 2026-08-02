# Prueba física — Unified U2/U3, edición transaccional de bloques

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

## Alcance

Esta fase activa dentro de `RuntimeUIFBDMapUnified`:

```text
U2 — EDITAR IN
U3 — EDITAR T
```

No hereda ni llama a ningún renderer Vx.

## Contrato de edición

```text
DETALLE + OK       -> abre el editor seleccionado
LEFT/RIGHT         -> cambia de campo
UP/DOWN            -> modifica el campo
OK                 -> valida y guarda
ESC                -> cancela sin modificar el motor
```

Los cambios usan `RuntimeUIV2EditSession`:

1. copia el programa a RAM;
2. modifica el borrador;
3. valida el programa completo;
4. detiene y recarga el motor únicamente al aplicar;
5. reinicia el motor si estaba en RUN.

No se escribe FRAM ni se conmutan salidas físicas.

## Compilación

- [ ] Compila con Arduino IDE.
- [ ] Compila con Arduino CLI `--clean`.
- [ ] Aparecen en el log:
  - [ ] `RuntimeUIFBDMapUnifiedEditInput.cpp`
  - [ ] `RuntimeUIFBDMapUnifiedEditTon.cpp`
  - [ ] `RuntimeUIFBDMapUnifiedEditorCommon.cpp`
- [ ] No hay símbolos duplicados.
- [ ] No hay referencias a V4/V5/V7/V13/V14 desde Unified.

## U2 — Entrada a EDITAR IN

Usar B07 TON y seleccionar `Trg`:

- [ ] OK abre `EDITAR IN`.
- [ ] No aparece barrido negro.
- [ ] No aparece el layout histórico `Bxx TIPO IN1/1`.
- [ ] Cabecera fila 1 conserva `B07 TON`.
- [ ] Cabecera fila 2 conserva `Trg`.
- [ ] RUN permanece estable.
- [ ] Se muestran dos campos: `FUENTE` y `LOGICA`.

## U2 — Navegación y valores

- [ ] LEFT/RIGHT alterna FUENTE <-> LOGICA.
- [ ] Solo cambian los marcos de los dos campos al mover el foco.
- [ ] UP/DOWN en FUENTE recorre:
  - [ ] X ABIERTO
  - [ ] HI CONST 1
  - [ ] LO CONST 0
  - [ ] bloques anteriores B00...Bxx
- [ ] Nunca ofrece el bloque actual ni bloques posteriores.
- [ ] UP/DOWN en LOGICA alterna DIRECTA/NEGADA.
- [ ] El texto `FUENTE` no parpadea al cambiar el valor.
- [ ] El texto `LOGICA` no parpadea al cambiar el valor.
- [ ] Solo se actualiza la línea del valor.

## U2 — Cancelar

- [ ] Cambiar fuente y lógica.
- [ ] Pulsar ESC.
- [ ] Retorna directamente a DETALLE.
- [ ] No aparece un frame histórico.
- [ ] La fuente original permanece intacta.
- [ ] El motor continúa RUN.

## U2 — Guardar

- [ ] Cambiar la fuente a una opción válida.
- [ ] Pulsar OK.
- [ ] Aparece `APLICANDO CAMBIOS...`.
- [ ] Retorna a DETALLE después de aplicar.
- [ ] La nueva fuente aparece en DETALLE.
- [ ] El cable y el estado lógico se actualizan.
- [ ] Regresar a MAPA muestra el layout actualizado.
- [ ] El motor continúa RUN.

Prueba adicional:

- [ ] Cambiar DIRECTA -> NEGADA y guardar.
- [ ] El pin muestra la inversión.
- [ ] La evaluación lógica se invierte correctamente.

## U3 — Entrada a EDITAR T

En B07 TON seleccionar `PARAM T`:

- [ ] OK abre `EDITAR T`.
- [ ] No aparece el editor histórico VALOR/UNIDAD.
- [ ] No aparece barrido negro.
- [ ] Cabecera conserva `B07 TON / PARAM T`.
- [ ] Se muestran SEG/CENT/BASE para base s.
- [ ] `Ta LECTURA` permanece visible.

## U3 — Navegación

- [ ] RIGHT recorre SEG -> CENT -> BASE.
- [ ] LEFT recorre en sentido inverso.
- [ ] Cada evento se procesa una sola vez.
- [ ] No salta dos campos.
- [ ] Solo cambian los marcos anterior y nuevo.

## U3 — Edición SEG/CENT

- [ ] Mantener UP/DOWN acelera el valor.
- [ ] Solo cambia `<nn>`.
- [ ] `SEG` permanece inmóvil.
- [ ] `CENT` permanece inmóvil.
- [ ] `Ta LECTURA` permanece inmóvil.
- [ ] El pie permanece inmóvil.
- [ ] `T CONFIGURADO` actualiza únicamente su valor.

## U3 — BASE exacta

Con `02:00s`:

- [ ] Cambiar s -> m produce `00:02m`.
- [ ] El tiempo real se conserva en 2000 ms.
- [ ] Los rótulos cambian una sola vez a MIN/SEG/BASE.
- [ ] Ta se representa una sola vez en la nueva base.

Intentar una base que no pueda representar exactamente el valor:

- [ ] No cambia la base.
- [ ] Aparece `BASE NO EXACTA`.
- [ ] El parámetro no se redondea.

## U3 — Ta vivo

Con TON detenido:

- [ ] El valor de Ta no parpadea.

Con TON temporizando:

- [ ] Solo cambia el valor visible de Ta.
- [ ] Los rótulos, campos y pie permanecen estables.

Al completar/apagar TON:

- [ ] Cambia color/valor una sola vez.
- [ ] No se reconstruye toda la pantalla.

## U3 — Cancelar

- [ ] Cambiar tiempo y BASE.
- [ ] Pulsar ESC.
- [ ] Retorna a DETALLE con `PARAM T` seleccionado.
- [ ] El parámetro original permanece intacto.

## U3 — Guardar

- [ ] Cambiar el tiempo a un valor reconocible.
- [ ] Pulsar OK.
- [ ] Aparece `APLICANDO CAMBIOS...`.
- [ ] Retorna a DETALLE.
- [ ] T muestra el valor guardado.
- [ ] El TON usa el nuevo tiempo durante el siguiente disparo.
- [ ] El motor continúa RUN.

## Regresión U1

- [ ] MAPA sigue operativo.
- [ ] DETALLE sigue operativo.
- [ ] Bloques apagados no parpadean.
- [ ] `Bxx TIPO` permanece estable.
- [ ] RUN permanece estable.
- [ ] El marco T-only continúa aprobado.

## Motor y almacenamiento

Esta fase no modifica:

```text
LogicV2BlockRecord = 12 bytes
LogicV2InputLink = 2 bytes
scan del motor
formato de programa
FRAM
salidas físicas
```

## Resultado

```text
Compilación:
EDITAR IN entrada:
EDITAR IN cancelar:
EDITAR IN guardar:
EDITAR T entrada:
EDITAR T valores:
EDITAR T base:
EDITAR T cancelar:
EDITAR T guardar:
Regresión U1:
Decisión: APROBAR / CORREGIR
```
