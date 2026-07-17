# Prueba — sesión de edición RAM v2

## Objetivo

Validar el backend transaccional que usará el editor gráfico de bloques antes de conectarlo a la TFT.

La prueba confirma que una modificación:

1. se realiza primero sobre una copia RAM;
2. no cambia el motor activo antes de confirmar;
3. valida el programa completo;
4. detiene, recarga y reinicia el motor al aplicar;
5. conserva el modelo v2 acíclico;
6. no escribe FRAM ni conmuta salidas físicas.

## Ejemplo

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_V2_EditSession_RAM
```

## Programa de prueba

```text
B00 I0.0 ──┐
            AND B02
B01 I0.1 ─o─┘
```

Estado inicial:

```text
I0.0 = 1
I0.1 = 0
IN2 negada
AND = 1
```

### Edición 1

Quitar la negación de `IN2`:

```text
B01 sin negar = 0
AND = 0
```

### Edición 2

Cambiar `IN2` a constante `HI`:

```text
I0.0 = 1
IN2 = HI
AND = 1
```

## Salida esperada

```text
PASS: carga programa base
PASS: arranca programa base
PASS: scan inicial
PASS: AND true con segunda entrada negada
PASS: crea borrador RAM
PASS: sesion activa
PASS: borrador inicialmente limpio
PASS: quita negacion de IN2 en borrador
PASS: borrador marcado como modificado
PASS: borrador valido
PASS: aplica borrador y reinicia motor
PASS: scan tras aplicar
PASS: AND false sin negacion
PASS: abre segundo borrador
PASS: cambia IN2 a constante HI
PASS: aplica fuente HI
PASS: scan con HI
PASS: AND true con I0.0 y HI

PASS=18 FAIL=0
```

## Criterio de aprobación

```text
COMPILA: sí
PASS: 18
FAIL: 0
MOTOR TRAS APLICAR: RUNNING
FRAM: sin escritura
SALIDAS FÍSICAS: sin conmutación
```

## Siguiente etapa

Con esta prueba aprobada se integra el primer editor gráfico:

```text
DETALLE
  RIGHT → EDITAR ENTRADA

EDITAR ENTRADA
  UP/DOWN → fuente X, HI, LO o bloque anterior
  LEFT     → normal / negada
  OK       → validar y aplicar
  RIGHT    → cancelar
```

La aplicación al motor debe ejecutarse desde `JWPLC_LogicRuntime_UI.update()`, fuera del callback gráfico de la TFT.
