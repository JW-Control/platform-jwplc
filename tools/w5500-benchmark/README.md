# JWPLC W5500 Benchmark Results

Este directorio contiene las pruebas de rendimiento, scripts y el driver RAW optimizado para el Ethernet W5500 del JWPLC Basic.

## Resumen de Resultados (2026-08-15)

Durante la investigación para optimizar la capa de red del JWPLC, se probaron distintas configuraciones de hardware y software sobre el bus SPI compartido. 

### 1. Frecuencia del bus SPI
- **Límite Físico Encontrado:** `20 MHz`.
- **Explicación:** A frecuencias de 30 MHz o superiores, el ESP32 transmite datos correctamente por MOSI, pero la lectura del canal MISO se corrompe por retardos de propagación (skew) debidos a las trazas de la PCB y la compartición del bus con la pantalla TFT, SD y FRAM. A 20 MHz, la integridad de la señal es 100% limpia y estable, lo que cumple el objetivo del hardware.

### 2. Optimización de Memoria (Window Size TCP)
- **Baseline (2KB RX / 2KB TX):** ~2.72 Mbps.
- **Buffers de 16KB:** Produjo saturación del bus SPI y caídas drásticas de velocidad a ~1.1 Mbps (y 0.6 Mbps al procesarse en bucles), provocado por la inyección masiva de ráfagas TCP desde el host PC y las ineficiencias de enrutamiento SPI de bloques extremos.
- **Punto Óptimo Encontrado (4KB RX / 4KB TX):** **~7.86 Mbps**. El equilibrio perfecto entre ventana TCP y capacidad de extracción del ESP32.

### 3. Conclusión
El throughput TCP RAW fue elevado desde los 2.6 Mbps originales (con el driver heredado a 14MHz) a **7.8 Mbps**, lo que representa una **mejora de casi el 300%** sin sobrepasar los márgenes eléctricos de seguridad de la placa, manteniendo total coexistencia con la pantalla gráfica y la FRAM.

## Contenido

- `firmware/benchmark_raw/`: El sketch de Arduino conteniendo el driver RAW `JWPLC_W5500` con el truco de exclusión DHCP y el bucle de prueba TCP.
- `scripts/bench_client.py`: Script en Python utilizado desde la PC para inyectar tráfico TCP máximo y probar caídas de conexión.

## Siguientes Pasos (Para Integración)
El código de `JWPLC_W5500` está listo para ser envuelto bajo las clases `Client` y `Server` de Arduino y reemplazar el viejo backend `JWPLC_Ethernet_W5x00_Backend` en futuras fases del core Alpha5.
