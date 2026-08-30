# Gate 7NB.3A-C — Soak async sincronizado + audio

Variante incremental del `20_alpha7_distributed_soak_2h_gate_7NB_async_master`.

## Objetivo

Validar dentro del acceptance completo que la secuencia de E/S sea gobernada por el Master y ejecutada de forma sincronizada en M2 y los Slaves, conservando el resto de periféricos y telemetría del soak 20.

Esta variante **no modifica `JWPLC_ModbusRTU`**. Consume la API Master cooperativa ya existente.

## Arquitectura I/O

El soak 20 original arrancaba un temporizador local en cada nodo. Aunque `IO_NODE_STAGGER_MS=0`, cada placa conservaba su propia fase y los relés terminaban audiblemente desincronizados.

En 7NB.3A-C:

1. M2 decide canal y fase ON/OFF.
2. M2 prepara cada Slave mediante FC06 direccionado, sin broadcast.
3. Cada Slave recibe máscara, canal y retardo relativo.
4. M2 y los Slaves programan la aplicación en una tarea `jwplcSyncIO` de prioridad alta en Core 1.
5. La tarea aplica el banco con `digitalWriteBlock(Q0_X, bitmap)` en el instante objetivo.
6. El loopback Q0.x -> I0.x sigue validando pulso, bitmap y mismatch.

La tarea dedicada evita que un bloqueo ocasional del loop Arduino por Ethernet desplace por cientos de milisegundos el instante de conmutación.

## Audio

Cada transición de salida dispara una nota corta no bloqueante usando LEDC. La nota depende del patrón:

- `Q0_0 .. Q0_7`: escala ascendente C5..C6.
- fase OFF: nota G4.
- si en el futuro se usan máscaras multibit, la cantidad de bits activos desplaza la nota, por lo que patrones distintos pueden producir notas distintas.

Patrones iguales en tres nodos producen la misma nota en los tres y deben escucharse como un solo evento.

### Volumen configurable

En el `.ino`:

```cpp
static constexpr uint8_t SOAK_BUZZER_VOLUME = 24; // 0..255
```

Referencia práctica para grabación:

| Valor | Uso |
|---:|---|
| 0 | mute |
| 18 | suave |
| 24 | recomendado para voz + test |
| 48 | fuerte |

El nivel se implementa con duty PWM LEDC; `tone()` por sí solo no permite controlar este nivel.

## Compile-check

Primero compilar sin subir a las placas. El sketch incluye como base el soak 20 mediante `#include` y renombra su `setup()/loop()` para conservar todas las rutas ya validadas.

Esperado al boot después de compilar/subir:

```text
[SYNC IO] apply task=PASS
GATE_RT_REV=4 7NB-sync-addressed-audio
BUZZER_VOLUME=24/255
```

Durante `START` debe aparecer:

```text
[SYNC IO] takeover=ON volume=24/255
```

## Criterio inicial PASS

- compile-check limpio;
- tarea `jwplcSyncIO = PASS` en los tres nodos;
- clicks M2/S1/S2 audiblemente en sincronía;
- notas audiblemente en sincronía;
- `MODBUS_CRC total/run = 0/0` en los tres;
- `MB_FAIL W/R/V = 0/0/0` durante la ventana corta;
- `IO mismatch = 0`;
- FRAM/RTC/SD/WiFi/Ethernet siguen activos como en soak 20.

Primera prueba física: 60-120 s. Si pasa, extender a 5-10 min antes de volver al endurance largo.

## Pendiente separado

El Gate Ethernet/Core1 sigue abierto. Esta variante desacopla la **aplicación temporal de las salidas** del loop bloqueable, pero no considera resuelto el bloqueo HTTP Ethernet observado (~200-450 ms y un pico cercano a 1 s). Ese problema se corrige en el gate Ethernet, no aquí.
