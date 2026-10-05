# Alpha14 — H4A0.4-P5 — Factibilidad DMA en el bus SPI compartido

Fecha: `2026-09-29`

## Resultado

```text
A14_H4A04P5_DMA_SHARED_BUS_FEASIBILITY=BLOCKED_SAFELY
P5_DMA_CANDIDATE_IMPLEMENTED=NO
PRODUCT_FAILURE=NO
PHYSICAL_STABILITY=PENDING_USER
NEXT=PHASE4_AVAILABLE_READ
```

P4 identificó `START_WAIT_EXCESS` como el mayor bloque accionable del helper
FIFO de 64 B. Antes de implementar DMA se revisó la implementación exacta
incluida en `jwplc_local:esp32 2.1.0-dev`, basada en ESP32 Arduino 3.3.8.

## Ownership actual

La librería canónica `SPIClass` obtiene un `spi_t *` mediante:

```text
spiStartBus(VSPI, ...)
```

El HAL Arduino:

- conserva su propio `spi_t` por bus;
- contiene el puntero de registros `spi_dev_t *`;
- crea su propio mutex;
- inicializa directamente registros, clock y FIFO;
- expone `spiTransferBytesNL()` como loop síncrono de máximo 64 B;
- no expone una primitiva DMA que reutilice ese mismo `spi_t`.

El package añade además dos capas que deben conservarse:

```text
jwplcSPI_acquire()/release()
SPIClass::beginTransaction()/endTransaction()
```

Todos los periféricos del producto comparten esas capas.

## Vía DMA disponible en ESP-IDF

La API pública DMA del ESP-IDF exige:

```text
spi_bus_initialize(host, bus_config, SPI_DMA_CH_AUTO)
spi_bus_add_device(host, device_config, &handle)
spi_device_polling_transmit(handle, &transaction)
```

Sus contratos confirman que:

- `spi_bus_initialize()` inicializa y reclama el host;
- devuelve `ESP_ERR_INVALID_STATE` si el host ya está en uso dentro de ese
  driver;
- `spi_bus_add_device()` requiere un bus inicializado por ese driver;
- las transferencias usan un `spi_device_handle_t`, no el `spi_t *` de
  Arduino;
- DMA añade sus propios recursos, ISR, buffers y bus lock.

Inicializar el driver ESP-IDF sobre `VSPI_HOST` mientras `SPIClass` posee y
configura el mismo periférico crearía dos modelos independientes de ownership
y podría reinicializar pines, registros, clock, CS o DMA. Esto viola las reglas
P5 y eleva el riesgo de corrupción de TFT/SD/FRAM/Ethernet.

## Alternativa de registros directos

Programar manualmente DMA desde la librería `SPI` requeriría, como mínimo:

- descriptores DMA y memoria compatible;
- reset/configuración de `dma_conf`;
- lifecycle de canal e interrupción;
- sincronización con el mutex HAL y el mutex global JWPLC;
- restauración completa del estado para todos los otros periféricos;
- tratamiento de buffers no alineados y longitudes parciales.

Eso constituye una re-arquitectura del bus, no una sola optimización P5. No se
puede validar de forma proporcionada como un A/B aislado durante esta sesión.

## Decisión

```text
SECOND_SPI_OWNER_ALLOWED=NO
SPI_BUS_REINITIALIZATION_ALLOWED=NO
HAND_ROLLED_DMA_WITHOUT_BUS_REARCHITECTURE=UNSAFE
P5_DMA_CANDIDATE_IMPLEMENTED=NO
P5_DMA_BLOCK_REASON=NO_SHARED_OWNERSHIP_BRIDGE_BETWEEN_ARDUINO_HAL_AND_IDF_DMA
```

No hay candidata de producto que rechazar porque no se implementó ni se
flasheó DMA. Se conserva `FIFO_REUSE` OFF por defecto y se pasa a la Fase 4
del roadmap: reducir overhead TCP por encima del payload mediante una API
interna/aditiva `available()`→`read()`.

```text
PHYSICAL_STABILITY=PENDING_USER
```
