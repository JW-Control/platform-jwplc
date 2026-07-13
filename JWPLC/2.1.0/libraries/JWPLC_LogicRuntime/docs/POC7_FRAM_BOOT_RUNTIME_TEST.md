# PoC 7 — arranque y ejecución del runtime desde FRAM

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

La prueba todavía usa una ventana reversible al final de la FRAM. No define el mapa final de producción.

## Ventana usada

```text
FRAM física:       8192 bytes
Ventana temporal:  0x1BC0..0x1FFF
Tamaño:            1088 bytes
Superblocks:       64 bytes
Slot A:            512 bytes
Slot B:            512 bytes
```

El contenido original se respalda en NVS antes de modificar la ventana.

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

## Flujo

### Etapa 1 — instalación

1. Subir `JWPLC_LogicRuntime_FRAM_Boot.ino`.
2. Abrir el monitor serial a 115200.
3. Escribir `START`.
4. El ejemplo respalda la ventana original en NVS.
5. Registra una instalación pendiente.
6. Formatea el layout A/B reducido.
7. Guarda, lee y valida el programa.
8. Registra el programa como listo.
9. Solicita reinicio.

Si hay un corte durante la instalación, el siguiente arranque detecta `STAGE_INSTALL_PENDING` y restaura la ventana original.

### Etapa 2 — ejecución después del reinicio

1. Reiniciar sin volver a compilar.
2. El ejemplo carga el programa activo desde FRAM.
3. Verifica metadatos y CRC mediante `LogicProgramStore` y `LogicProgramCodec`.
4. Reconstruye el programa en `LogicProgramBuffer`.
5. Inicializa `JWPLC_LogicRuntime`.
6. Carga el programa en el motor.
7. Inicia el scan físico.
8. Probar las entradas y `Q0_0`.

Mientras el runtime está activo, un reinicio vuelve a cargar y ejecutar el mismo programa persistente.

### Etapa 3 — restauración

1. Escribir `RESTORE` por Serial.
2. El runtime se detiene.
3. Todas las salidas se apagan.
4. Se registra restauración pendiente.
5. Se restaura la ventana original.
6. Se relee y verifica byte por byte y por CRC32.
7. Se elimina el estado temporal de NVS.

Si hay un corte durante la restauración, el siguiente arranque vuelve a intentar restaurarla.

## Incidente detectado en la primera ejecución

La instalación y la carga desde FRAM fueron correctas, pero el primer `tick()` terminó con:

```text
PROGRAM_EXECUTION_FAILED
```

Causa identificada:

- `LogicProgramBuffer::asProgram()` devuelve un descriptor `LogicProgram` por valor.
- El motor guardaba un puntero al descriptor recibido por referencia.
- Al finalizar `loadProgram()`, el descriptor temporal dejaba de existir.
- Los arreglos `name` y `blocks` seguían siendo válidos, pero el puntero al descriptor quedaba colgante.

Corrección aplicada:

- `LogicEngine` conserva una copia interna del descriptor `LogicProgram`.
- La copia mantiene los punteros hacia `name` y `blocks` del buffer externo.
- `LogicProgramBuffer` debe seguir vivo durante la ejecución, pero ya no es necesario que el descriptor devuelto por `asProgram()` tenga vida permanente.
- Ante cualquier fallo de scan, las salidas continúan apagándose y el runtime entra en `FAULT`.

## Resultado después de la corrección

El mismo programa persistido previamente se cargó sin reinstalarlo ni borrar NVS:

```text
[PASS] Programa cargado desde FRAM hacia RAM.
[PASS] Runtime iniciado con el programa persistente.
```

El runtime permaneció ejecutándose durante aproximadamente un minuto sin fallos:

```text
I0_0=0 I0_1=0 AND=0 TON=0 Q0_0=0
scan mínimo:   4 us
scan promedio: 5 us
scan máximo:   425 us
escrituras Q0: 2 y estable
```

La prueba terminó mediante `RESTORE`:

```text
Runtime detenido. Q0 apagadas.
[PASS] Contenido original de FRAM restaurado exactamente.
[PASS] Estado temporal NVS eliminado.
ARRANQUE Y EJECUCION DESDE FRAM: PASS
PoC 7 completada.
```

Validado con este resultado:

- persistencia y carga desde FRAM;
- reconstrucción del programa en RAM;
- corrección del descriptor temporal;
- ejecución estable del scan;
- ausencia de escrituras Q0 redundantes;
- parada segura;
- restauración exacta de FRAM y limpieza de NVS.

Pendiente para cierre funcional completo:

- registrar una transición con `I0_0=1` e `I0_1=0` durante dos segundos;
- confirmar `TON=1` y `Q0_0=1` desde el programa cargado desde FRAM;
- confirmar el apagado al activar `I0_1`.

## Criterios de aprobación

- El programa queda persistido y validado antes del reinicio.
- Tras reiniciar, el programa se carga desde FRAM sin una definición fija alternativa.
- El runtime ejecuta la lógica física esperada.
- `Q0_0` conserva el comportamiento del `TON` probado en PoC anteriores.
- Las salidas se apagan antes de restaurar.
- La ventana original se recupera exactamente.
- El resultado final muestra:

```text
ARRANQUE Y EJECUCION DESDE FRAM: PASS
PoC 7 completada.
```

## Alcance excluido

- Mapa completo definitivo de la FRAM de 8 KiB.
- Autoinstalación del programa predeterminado de producción.
- API final de alto nivel para guardar y activar programas.
- Retentivos persistentes.
- Editor frontal.
- Actualización remota de programa.
- Declaración de hard real-time.
