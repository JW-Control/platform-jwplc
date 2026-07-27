# PoC 7 — arranque y ejecución del runtime desde FRAM

## Estado

```text
VALIDADA Y CERRADA
ARRANQUE Y EJECUCION DESDE FRAM: PASS
```

## Objetivo

Validar el flujo completo:

```text
Programa binario en FRAM
        ↓
Carga y validación durante el arranque
        ↓
Reconstrucción en RAM
        ↓
JWPLC_LogicRuntime::loadProgram()
        ↓
Ejecución física de E/S
```

La prueba usa una ventana reversible al final de la FRAM. No define todavía el mapa final de producción.

## Ventana utilizada

```text
FRAM física:       8192 bytes
Ventana temporal:  0x1BC0..0x1FFF
Tamaño:            1088 bytes
Superblocks:       64 bytes
Slot A:            512 bytes
Slot B:            512 bytes
```

El contenido original se respalda en NVS antes de modificar la ventana y se restaura al finalizar.

## Programa persistente

```text
I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0
```

Bloques:

```text
B0 = I0_0
B1 = I0_1
B2 = NOT B1
B3 = B0 AND B2
B4 = TON(B3, 2000 ms)
B5 = Q0_0 <- B4
```

## Flujo validado

### Instalación

1. El ejemplo respaldó la ventana original en NVS.
2. Registró una instalación pendiente.
3. Formateó el layout A/B reducido.
4. Guardó el programa en FRAM.
5. Lo releyó y validó.
6. Registró el programa como listo.

Resultado:

```text
[PASS] Programa persistido y verificado en FRAM.
INSTALACION COMPLETA.
```

### Arranque desde FRAM

Después de un reinicio completo del ESP32:

```text
[PASS] Programa cargado desde FRAM hacia RAM.
[PASS] Runtime iniciado con el programa persistente.
```

El programa ejecutado no se tomó de una definición fija alternativa. Se reconstruyó desde la imagen activa almacenada en FRAM.

### Ejecución física

Estado inicial:

```text
I0_0=0 I0_1=0 AND=0 TON=0 Q0_0=0
```

Activación de `I0_0` con `I0_1=0`:

```text
I0_0=1 I0_1=0 AND=1 TON=0 Q0_0=0
I0_0=1 I0_1=0 AND=1 TON=1 Q0_0=1
```

Se confirmó:

- inicio del `TON` al cumplirse la condición;
- permanencia de `Q0_0` apagada antes de completar dos segundos;
- activación de `TON` y `Q0_0` después del retardo;
- una sola escritura física adicional cuando Q0 cambió a encendida.

Al retirar `I0_0`:

```text
I0_0=0 I0_1=0 AND=0 TON=0 Q0_0=0
```

Se confirmó:

- reinicio del temporizador;
- apagado de `Q0_0`;
- una sola escritura física adicional cuando Q0 cambió a apagada.

La función de inhibición mediante `I0_1` ya había sido validada en las PoC físicas anteriores con el mismo motor y la misma definición lógica. En PoC 7 se validó específicamente el recorrido completo FRAM → RAM → motor → E/S física.

## Rendimiento observado

Durante la ejecución desde FRAM:

```text
scan mínimo:    4 us
scan promedio:  5 us
scan máximo:  460 us
```

El máximo apareció durante una transición física de Q0, coherente con el costo de escritura del banco por I2C.

Contador de escrituras:

```text
Q0 estable apagada: 2
Q0 pasa a encendida: 3
Q0 pasa a apagada:  4
```

No se observaron escrituras redundantes mientras el estado de salida permanecía estable.

## Incidente detectado y corregido

La primera ejecución cargó correctamente la imagen desde FRAM, pero el primer `tick()` terminó con:

```text
PROGRAM_EXECUTION_FAILED
```

Causa:

- `LogicProgramBuffer::asProgram()` devuelve un descriptor `LogicProgram` por valor;
- `LogicEngine` conservaba un puntero al descriptor recibido por referencia;
- al finalizar `loadProgram()`, el descriptor temporal dejaba de existir;
- los buffers `name` y `blocks` seguían siendo válidos, pero el puntero al descriptor quedaba colgante.

Corrección:

- `LogicEngine` conserva ahora una copia interna del descriptor `LogicProgram`;
- la copia mantiene los punteros hacia `name` y `blocks` del buffer externo;
- `LogicProgramBuffer` debe seguir vivo durante la ejecución;
- ya no se depende de la vida del descriptor temporal devuelto por `asProgram()`.

La corrección se validó reutilizando el mismo programa persistido, sin reinstalarlo ni borrar NVS.

## Restauración final

La prueba terminó mediante `RESTORE`:

```text
Runtime detenido. Q0 apagadas.
[PASS] Contenido original de FRAM restaurado exactamente.
[PASS] Estado temporal NVS eliminado.

ARRANQUE Y EJECUCION DESDE FRAM: PASS
PoC 7 completada.
```

## Conclusiones

Quedó validado:

- almacenamiento binario del programa en FRAM;
- selección del slot activo mediante el gestor A/B;
- persistencia entre reinicios completos;
- reconstrucción del programa en RAM;
- entrega segura del programa reconstruido al motor;
- ejecución física de la lógica almacenada;
- temporización `TON` correcta;
- activación y apagado físico de `Q0_0`;
- ausencia de escrituras Q0 redundantes;
- parada segura;
- restauración exacta de la FRAM;
- limpieza del estado temporal en NVS.

## Alcance excluido

- mapa definitivo de producción de la FRAM de 8 KiB;
- API final de alto nivel para instalar, guardar y activar programas;
- autoinstalación del programa predeterminado de producción;
- retentivos persistentes;
- editor frontal;
- actualización remota de programa;
- declaración de hard real-time.
