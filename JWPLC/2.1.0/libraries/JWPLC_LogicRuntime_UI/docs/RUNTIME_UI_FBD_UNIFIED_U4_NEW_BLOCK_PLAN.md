# U4 — NUEVO BLOQUE en RuntimeUIFBDMapUnified

## Estado

```text
EN IMPLEMENTACIÓN / NO LISTO PARA PRUEBA FÍSICA
```

## Objetivo

Migrar la creación guiada de bloques a `RuntimeUIFBDMapUnified` sin heredar ni llamar a V4...V14.

## Tipos incluidos

```text
ENTRADA DI
NOT
AND 2
TON
SALIDA DO
```

No se amplía todavía el catálogo del motor.

## Flujo

```text
MAPA FBD
  -> seleccionar nodo (+)
  -> OK
NUEVO BLOQUE / TIPO
  -> UP/DOWN selecciona tipo
  -> OK avanza
  -> ESC vuelve al nodo (+)
CONFIGURAR
  -> LEFT/RIGHT cambia campo
  -> UP/DOWN cambia valor
  -> OK crea
  -> ESC vuelve a TIPO
APLICANDO
  -> sesión RAM valida y recarga
MAPA FBD
  -> nuevo bloque seleccionado
```

## Política visual

- No usar `clearScreen()` ni `fillScreen()` durante transiciones.
- Mantener cabecera negra unificada.
- No borrar RUN si el estado no cambia.
- El nodo (+) nunca forma parte del programa ni consume un bloque.
- El nodo (+) se dibuja después del último nivel lógico sin traslaparse.
- Las listas y campos actualizan solamente las regiones que cambian.
- El pie se repinta únicamente al cambiar el mensaje.

## Valores configurables

### ENTRADA DI

```text
RECURSO: I0.0 ... I0.7
```

### NOT

```text
FUENTE
```

### AND 2

```text
IN1
IN2
```

IN2 permite `X ABIERTO`.

### TON

```text
FUENTE
SEG/MIN/HORA
CENT/SEG/MIN
BASE s/m/h
```

La BASE usa la decisión tipo LOGO!: conserva los dos números y cambia su interpretación. El segundo campo se limita a 59 al entrar a minutos u horas.

### SALIDA DO

```text
FUENTE
RECURSO: Q0.0 ... Q0.7
```

## Aplicación transaccional

1. `RuntimeUIV2EditSession::begin()` copia el programa.
2. Se construyen enlaces y parámetros en RAM.
3. `appendBlock()` agrega al final del orden topológico.
4. `validate()` verifica el programa completo.
5. `apply(true)` recarga el motor conservando RUN cuando corresponda.
6. Si falla, el programa activo permanece intacto.

No se escribe FRAM ni se conmutan salidas físicas desde la UI.

## Criterios de prueba

- [ ] RIGHT desde un bloque del último nivel selecciona (+).
- [ ] (+) no traslapa el último bloque.
- [ ] LEFT o ESC abandona (+).
- [ ] OK abre TIPO directamente, sin layout residual.
- [ ] UP/DOWN recorre los cinco tipos.
- [ ] LEFT no sale de TIPO.
- [ ] ESC vuelve a (+).
- [ ] CONFIGURAR muestra solo los campos del tipo.
- [ ] ESC en CONFIGURAR vuelve a TIPO.
- [ ] DI crea el recurso elegido.
- [ ] NOT crea la fuente elegida.
- [ ] AND2 crea ambas fuentes.
- [ ] TON crea fuente, tiempo y base.
- [ ] DO crea fuente y recurso.
- [ ] El bloque creado queda seleccionado.
- [ ] MAPA y DETALLE reflejan el bloque nuevo.
- [ ] Un error de validación no altera el programa activo.
- [ ] Ninguna transición produce barrido negro completo.

## Activación

No conectar U4 a la fachada preview hasta completar:

```text
modelo
nodo (+)
render TIPO
render CONFIGURAR
entrada de botones
append transaccional
retorno a MAPA
```
