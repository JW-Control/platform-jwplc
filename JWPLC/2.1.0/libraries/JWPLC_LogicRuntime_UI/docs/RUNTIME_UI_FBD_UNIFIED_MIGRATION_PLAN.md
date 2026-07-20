# Plan de consolidación — RuntimeUIFBDMapUnified

## Estado

```text
INICIADO / NO ACTIVO TODAVÍA
```

La implementación activa actual continúa siendo:

```text
RuntimeUIFBDMap
  -> RuntimeUIFBDMapActiveRenderer
  -> RuntimeUIFBDMapV14
  -> ... capas históricas Vx
```

La nueva implementación es independiente:

```text
RuntimeUIFBDMapUnified
```

No hereda ni llama a:

```text
RuntimeUIFBDMapV4
RuntimeUIFBDMapV5
RuntimeUIFBDMapV7
RuntimeUIFBDMapV13
RuntimeUIFBDMapV14
RuntimeUIFBDMapActiveRenderer
```

No debe activarse como fachada pública hasta alcanzar paridad funcional y pasar validación física.

## Motivo

La cadena histórica permitió iterar rápidamente, pero hoy produce deuda técnica:

- múltiples propietarios de cabecera;
- múltiples rutas de entrada/salida;
- renderers antiguos que dibujan frames provisionales;
- `clearScreen()` heredados en algunas transiciones;
- correcciones posteriores que tapan, pero no eliminan, layouts anteriores;
- dificultad para añadir nuevos bloques y eliminación sin introducir regresiones.

Casos observados físicamente:

- `Bxx TON IN1/1` residual al entrar o retornar desde `EDITAR IN`;
- traslape de `EDITAR IN` con contexto de una sola fila;
- retorno a DETALLE conservando temporalmente título/contexto antiguos;
- barrido negro en `DETALLE -> EDITAR IN`;
- diferencias entre `MAPA -> DETALLE` y `DETALLE -> MAPA`.

## Principios obligatorios

1. Una sola máquina de estados.
2. Una sola cabecera para MAPA, DETALLE y EDITAR.
3. Una sola función de transición `transitionTo()`.
4. Ninguna transición interna FBD usa `clearScreen()` o `fillScreen()`.
5. El estado RUN/READY/FAULT no se limpia si no cambia.
6. El contexto `Bxx TIPO / elemento` no se limpia si no cambia.
7. La limpieza del contenido ocurre en una única función central.
8. Entrada y render pertenecen a la misma clase.
9. Las cachés se definen por región visual, no por revisión histórica.
10. El motor v2 y la sesión transaccional permanecen separados de la UI.

## Máquina de estados única

```text
Map
Detail
EditInput
EditTon
AddType
Configure
SourceList
SourceEdit
ParameterList
ParameterEdit
DeleteConfirm
```

Cada vista implementa:

```text
handle<View>Input()
render<View>()
```

Las transiciones se controlan exclusivamente mediante:

```text
transitionTo(previous, next)
leaveView(previous, next)
clearTransitionRegions(previous, next)
enterView(previous, next)
```

## Cabecera unificada

Regiones fijas:

```text
Título:       x=0..107
Contexto:     x=108..232
Estado:       x=239..313
Fila 1:       y=3
Fila 2:       y=12
```

Ejemplos:

```text
MAPA FBD     B07 TON          RUN
             ON

DETALLE      B07 TON          RUN
             Trg

DETALLE      B07 TON          RUN
             PARAM T

EDITAR IN    B07 TON          RUN
             Trg

EDITAR T     B07 TON          RUN
             PARAM T
```

El título, contexto y estado tienen cachés independientes.

## Política general de transición

Dentro del FBD:

```text
No usar clearScreen().
No usar fillScreen().
No limpiar RUN si el estado no cambia.
No limpiar Bxx TIPO si el bloque no cambia.
No dibujar una vista histórica para luego corregirla.
```

La región de contenido compartida es:

```text
x=3..316
y=28..167
```

La primera implementación puede sustituir esa región completa con `COLOR_PANEL`, pero siempre dibujando inmediatamente el contenido final. Después se optimizarán pares de vistas con regiones menores.

## Fases de migración

### Fase U0 — núcleo independiente

- [x] Crear `RuntimeUIFBDMapUnified.h`.
- [x] Crear `RuntimeUIFBDMapUnified.cpp`.
- [x] Máquina de estados única.
- [x] Regiones fijas de cabecera y contenido.
- [x] Política explícita sin `clearScreen()`.
- [x] Sesión transaccional enlazada al mismo motor v2 del ReadModel.
- [ ] Compilación del package con clase no activa.

### Fase U1 — MAPA y DETALLE

- [ ] Migrar layout del mapa.
- [ ] Migrar navegación LEFT/RIGHT/UP/DOWN.
- [ ] Migrar selección y ventana horizontal.
- [ ] Migrar DETALLE.
- [ ] Unificar `MAPA <-> DETALLE` con transición simétrica.
- [ ] Cabecera estable sin `IN1/1` residual.
- [ ] Validación física.

### Fase U2 — EDITAR IN

- [ ] Migrar sesión de fuente/lógica.
- [ ] `DETALLE -> EDITAR IN` sin barrido negro.
- [ ] `EDITAR IN -> DETALLE` sin título residual.
- [ ] Cabecera `Bxx TIPO / Trg` estable.
- [ ] Aplicar/cancelar transaccionalmente.
- [ ] Validación física.

### Fase U3 — EDITAR T

- [ ] Migrar base tipo LOGO!.
- [ ] Migrar SEG/CENT/BASE.
- [ ] Migrar T/Ta con cachés independientes.
- [ ] Migrar repeat acelerado.
- [ ] Validación física.

### Fase U4 — asistente de creación

- [ ] Migrar nodo `+`.
- [ ] Migrar selección de tipo.
- [ ] Migrar CONFIGURAR.
- [ ] Migrar fuentes y parámetros.
- [ ] Mantener cuatro filas y contador `01/08`.
- [ ] Validación física.

### Fase U5 — activación

- [ ] Cambiar `RuntimeUIFBDMap` para contener o heredar solo de Unified.
- [ ] Compilar Arduino IDE.
- [ ] Compilar Arduino CLI.
- [ ] Validar todas las pantallas físicamente.
- [ ] Confirmar que ningún archivo `RuntimeUIFBDMapVx` está incluido por la fachada.
- [ ] Congelar o mover Vx a `legacy/` sin borrarlos todavía.

### Fase U6 — nuevas funciones

Solo después de activar Unified:

- [ ] `ELIMINAR BLOQUE` sin cascada.
- [ ] OR.
- [ ] NAND.
- [ ] NOR.
- [ ] XOR.
- [ ] SET/RESET.
- [ ] Cantidad de entradas 2..8.
- [ ] TOF/TP/contadores en etapas posteriores del motor.

## Criterios de activación

Unified no reemplaza la implementación activa hasta cumplir:

```text
MAPA:                  APROBADO
DETALLE:               APROBADO
EDITAR IN:             APROBADO
EDITAR T:              APROBADO
NUEVO BLOQUE:          APROBADO
ESC jerárquico:        APROBADO
Botonera con Trg OFF:  APROBADA
Sin barridos negros:   APROBADO
Sin cabeceras dobles:  APROBADO
Aplicación RAM:        APROBADA
```

## Qué no cambia

Esta consolidación no modifica:

```text
LogicV2EnginePrototype
LogicV2BlockRecord = 12 bytes
LogicV2InputLink = 2 bytes
parameter
resource
scan TON
FRAM
salidas físicas
```

## Decisión

```text
No añadir una V15.
No continuar corrigiendo transiciones en la cadena Vx.
Todo desarrollo visual nuevo se implementa en RuntimeUIFBDMapUnified.
La implementación anterior se mantiene solo como referencia y fallback hasta validar paridad.
```
