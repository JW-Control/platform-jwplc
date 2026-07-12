# PoC 5 — Backend físico sobre `JW_FRAM`

## Objetivo

Validar el gestor A/B sobre la FRAM física del JWPLC Basic sin ocupar todavía el layout definitivo de 5184 bytes ni dejar datos persistentes de prueba.

## Alcance

El ejemplo usa una ventana reducida al final de la FRAM actual de 8 KiB:

```text
FRAM física:       0x0000..0x1FFF  (8192 bytes)
Ventana PoC 5:     0x1BC0..0x1FFF  (1088 bytes)
Superblocks:       64 bytes
Slot A reducido:   512 bytes
Slot B reducido:   512 bytes
```

La ventana se direcciona de forma relativa mediante `LogicFRAMStorage`, por lo que el gestor A/B continúa viendo una memoria iniciada en cero.

## Backend

`LogicFRAMStorage` implementa la interfaz `LogicByteStorage` sobre una instancia existente de `JW_FRAM`.

Responsabilidades:

- validar la capacidad física reportada;
- aplicar un desplazamiento base;
- impedir accesos fuera de la ventana;
- ejecutar `READ` mediante `JW_FRAM::read()`;
- envolver escrituras con `WREN` y `WRDI`;
- rellenar regiones en bloques de 64 bytes;
- conservar el bloqueo de bus SPI ya inyectado por `JWPLC_GlobalPeripherals`.

## Seguridad de la prueba

El ejemplo `JWPLC_LogicRuntime_FRAM_Storage`:

1. Comprueba que la FRAM global reporta 8192 bytes.
2. No inicializa el runtime de E/S.
3. No conmuta salidas.
4. Espera la confirmación exacta `ERASE` por Serial.
5. Respalda los 1088 bytes de la ventana en RAM.
6. Ejecuta el flujo A/B.
7. Restaura los 1088 bytes originales.
8. Relee y compara byte por byte.
9. Verifica además el CRC32 del respaldo restaurado.

## Advertencia

El respaldo vive únicamente en RAM. Un reset o corte de alimentación durante la ejecución impediría restaurar automáticamente el contenido anterior.

Por ello:

- usar una unidad de prueba;
- no cortar alimentación;
- no pulsar reset;
- no cerrar la prueba hasta ver la confirmación de restauración.

## Programas usados

### Programa A

```text
I0_0 -> NOT -> Q0_0
```

Metadatos:

```text
programId: 0xA001
generation: 1
```

### Programa B

```text
I0_1 -> Q0_1
```

Metadatos:

```text
programId: 0xB002
generation: 2
```

Los programas se almacenan y reconstruyen, pero no se ejecutan físicamente.

## Resultado esperado

```text
BACKEND FRAM FISICA: PASS
La ventana de prueba fue restaurada.
```

Criterios:

- backend inicializado;
- FRAM de 8 KiB detectada;
- ventana correcta;
- layout reducido válido;
- respaldo correcto;
- Programa A guardado y cargado;
- Programa B guardado y cargado;
- metadatos conservados;
- ambos programas superan el validador;
- restauración exacta del contenido previo.

## Qué no valida todavía

- Layout definitivo de 2560 bytes por slot.
- Imagen máxima de 100 bloques en FRAM real.
- Corte físico de alimentación.
- Recuperación después de reset real.
- Escrituras concurrentes con una operación SD prolongada.
- Cambio a FRAM de 32 KiB.

## Siguiente etapa

Después de aprobar PoC 5:

1. Reservar explícitamente el mapa completo de runtime.
2. Ejecutar guardado/carga con slots de 2560 bytes.
3. Separar modo `format`, `install` y `boot`.
4. Probar reinicio físico entre guardado y carga.
5. Preparar cortes controlados en puntos transaccionales.
6. Integrar la carga del programa activo con `JWPLC_LogicRuntime_Default.ino`.
