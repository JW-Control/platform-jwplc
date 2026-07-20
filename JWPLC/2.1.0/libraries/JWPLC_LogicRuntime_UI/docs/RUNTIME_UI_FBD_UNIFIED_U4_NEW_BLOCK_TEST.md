# Prueba física — Unified U4 NUEVO BLOQUE

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN ARDUINO Y VALIDACIÓN FÍSICA
```

## Base validada

Antes de U4 quedaron aprobados físicamente:

- MAPA FBD y DETALLE en `RuntimeUIFBDMapUnified`.
- Transiciones regionales sin barrido negro completo.
- EDITAR IN transaccional.
- EDITAR T tipo LOGO!.
- Cambio libre de BASE `s / m / h`.
- Cabecera negra continua y pie inferior estable.

## Alcance de U4

U4 agrega un asistente independiente para crear bloques al final del programa RAM:

```text
MAPA FBD
  -> nodo virtual +
  -> NUEVO / TIPO
  -> CONFIGURAR
  -> FUENTES o PARAMETROS
  -> CREAR
  -> validación transaccional
  -> MAPA con el bloque nuevo seleccionado
```

Tipos iniciales:

```text
ENTRADA DI
NOT
AND 2
TON
SALIDA DO
```

No se reutilizan V4...V14. El fallback legacy continúa disponible y el ejemplo preview activa Unified explícitamente.

## Contrato de navegación

### Nodo virtual +

- [ ] RIGHT desde un bloque del último nivel selecciona `+` cuando hay capacidad.
- [ ] El `+` seleccionado ocupa una columna virtual completa.
- [ ] No se traslapa con el último bloque.
- [ ] LEFT retorna al bloque de origen.
- [ ] ESC retorna al bloque de origen.
- [ ] OK abre NUEVO.

El preview compacto solo aparece si existe una columna completamente libre. Con cinco o más niveles puede omitirse para evitar traslapes.

### NUEVO / TIPO

- [ ] UP/DOWN recorre DI, NOT, AND 2, TON y DO.
- [ ] La cabecera actualiza `Bxx TIPO` sin residuos.
- [ ] LEFT no retrocede ni cambia de pantalla.
- [ ] ESC es la única salida hacia MAPA.
- [ ] OK abre CONFIGURAR.

### CONFIGURAR

- [ ] LEFT/RIGHT recorre únicamente grupos habilitados.
- [ ] FUENTES queda deshabilitado para DI.
- [ ] PARAMETROS queda deshabilitado para NOT y AND 2.
- [ ] CREAR siempre es seleccionable.
- [ ] ESC vuelve a NUEVO conservando el programa original.
- [ ] No aparece un barrido negro completo.

## Pruebas por tipo

### ENTRADA DI

- [ ] Solo PARAMETROS y CREAR son seleccionables.
- [ ] RECURSO recorre las entradas digitales disponibles.
- [ ] ESC cancela el cambio de recurso.
- [ ] OK acepta el recurso.
- [ ] CREAR agrega el DI y retorna al MAPA.

### NOT

- [ ] FUENTES muestra una entrada.
- [ ] La fuente recorre ABIERTO, HI, LO y bloques existentes.
- [ ] El mini FBD destaca la fuente cuando corresponde.
- [ ] CREAR agrega el NOT con la fuente elegida.

### AND 2

- [ ] FUENTES muestra IN1 e IN2.
- [ ] UP/DOWN cambia entre ambas entradas.
- [ ] Cada entrada conserva su fuente de manera independiente.
- [ ] CREAR agrega el AND con dos enlaces.

### TON

- [ ] FUENTES muestra Trg.
- [ ] PARAMETROS muestra T.
- [ ] EDITAR T permite SEG/CENT/BASE.
- [ ] BASE cambia libremente `s -> m -> h`.
- [ ] El segundo campo se limita a 59 en m/h sin bloquear.
- [ ] El pie inferior permanece estable.
- [ ] CREAR agrega el TON con `parameter` y `resource` correctos.

### SALIDA DO

- [ ] FUENTES muestra una entrada.
- [ ] PARAMETROS muestra un Q disponible.
- [ ] No propone una salida ya usada por otro bloque DO.
- [ ] Si no queda Q disponible, CREAR falla sin modificar el programa.
- [ ] CREAR agrega el DO con fuente y recurso elegidos.

## Aplicación transaccional

- [ ] CREAR abre una copia RAM mediante `RuntimeUIV2EditSession`.
- [ ] `appendBlock()` prepara el bloque nuevo.
- [ ] `validate()` comprueba el programa completo.
- [ ] La aplicación ocurre fuera del callback TFT.
- [ ] Ante error se cancela el borrador.
- [ ] Ante éxito el bloque nuevo queda seleccionado en MAPA.
- [ ] El bloque nuevo puede abrir DETALLE y sus editores.

## Regresión visual

- [ ] RUN permanece sobre fondo negro continuo.
- [ ] La cabecera usa dos filas sin traslapes.
- [ ] Los bloques apagados no parpadean al navegar.
- [ ] El pie inferior no parpadea.
- [ ] No reaparecen layouts Vx históricos.
- [ ] MAPA <-> DETALLE conserva las transiciones ya aprobadas.

## Alcance excluido

U4 no implementa:

```text
FRAM
persistencia de proyectos
salidas físicas desde la UI
eliminación de bloques
nuevos tipos adicionales
entradas variables de 2 a 8
```

## Criterio de cierre

```text
Compilación Arduino:       APROBADA / FALLA
Nodo +:                    APROBADO / FALLA
NUEVO / TIPO:              APROBADO / FALLA
CONFIGURAR:                APROBADO / FALLA
DI:                        APROBADO / FALLA
NOT:                       APROBADO / FALLA
AND 2:                     APROBADO / FALLA
TON:                       APROBADO / FALLA
DO:                        APROBADO / FALLA
Cancelación transaccional: APROBADA / FALLA
Creación transaccional:    APROBADA / FALLA
Regresión U1-U3:           APROBADA / FALLA
Decisión:                  ACTIVAR UNIFIED / CORREGIR U4
```
