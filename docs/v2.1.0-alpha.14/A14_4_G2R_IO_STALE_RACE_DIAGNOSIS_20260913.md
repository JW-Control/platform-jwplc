# Alpha14 A14.4-G2R — Diagnóstico de `IO_STALE` espurio

Fecha: 2026-09-13

## Contexto

Durante A14.4-G2 se validó tráfico físico simultáneo:

- Modbus TCP Server en el DUT a 500 req/s, FC03/125.
- Modbus RTU Slave ID 2 en el DUT, 115200 8N1.
- Segundo JWPLC como RTU Master, FC03/16 con ciclo objetivo de 20 ms.
- Full runtime activo: HMI Dirty/On-Demand, SD, FRAM, RTC, botonera, TCA/I/O y probe SPI.

Resultados de comunicaciones:

- TCP: 30000/30000, 99.9998 %, 0 timeouts, 0 errores de transporte, 0 errores de protocolo y 0 bus-lock timeouts.
- RTU Master: 3000/3000, 100 % de éxito, 0 CRC, 0 timeouts, 0 verify fails.
- RTU efectivo: 49.597 Hz.
- DUT RTU: 2997 RX / 2997 TX / 2997 OK, 0 CRC, 0 excepciones.

El gate quedó inicialmente en `REVIEW` porque el runner calculó `FULL_RUNTIME_CLEAN=NO`.

## Recuperación G2R

El snapshot acumulado identificó únicamente:

- `IO_STALE=1`
- `PERIPHERAL_FAILURE_COUNT=1`
- `IO_MAX_AGE_MS=4294967295`

El resto de periféricos permaneció limpio:

- SD append fails = 0
- SD verify fails = 0
- FRAM fails = 0
- RTC stale = 0
- botones not-ready = 0
- SPI probe fails = 0
- RTU CRC = 0
- RTU exceptions = 0
- RTU timeouts = 0

Además, al momento del snapshot:

- `IO_INITIALIZED=YES`
- `IO_CURRENT_AGE_MS=12`

## Causa raíz

El benchmark `a14_perf_full_runtime_realistic.ino` calcula actualmente la edad de I/O de esta forma:

```cpp
static void sampleIo(uint32_t now)
{
    const JWPLC_IOState *io = jwplcGetIOState();
    ...
    const uint32_t age = (uint32_t)(now - io->last_scan_ms);
    ...
}
```

`now` se captura antes en `serviceRealisticWorkload()`.

Sin embargo, el runtime JWPLC ejecuta `jwplcSystemScanIO()` desde una tarea FreeRTOS separada (`jwplcSystemTask`) con prioridad normal 2, mientras el loop del sketch se ejecuta en su propia tarea.

Por lo tanto existe una carrera legítima de lectura del diagnóstico:

1. el benchmark captura `now = millis()`;
2. la tarea del sistema ejecuta un nuevo ScanIO;
3. `g_ioState.last_scan_ms` se actualiza a `millis()` y puede quedar 1 ms por delante del `now` previamente capturado;
4. el benchmark evalúa `now - last_scan_ms` como resta unsigned;
5. un desfase de -1 ms se convierte en `UINT32_MAX = 4294967295`;
6. el benchmark incrementa falsamente `IO_STALE`.

El valor observado `IO_MAX_AGE_MS=4294967295` es exactamente consistente con este escenario.

## Clasificación

### Comunicaciones A14.4-G2

`PASS_PHYSICAL`

Se demostró físicamente coexistencia simultánea de TCP y RTU sin pérdida de solicitudes procesadas:

- TCP 30000/30000.
- RTU 3000/3000.
- 0 errores de comunicaciones.

### G2R

`PASS_DIAGNOSTIC`

La única condición que provocó `FULL_RUNTIME_CLEAN=NO` fue un falso positivo de la instrumentación de freshness de I/O.

No existe evidencia de que el TCA/I/O haya quedado realmente stale.

## Corrección recomendada para el benchmark

No modificar todavía la API ni el runtime de producción.

Corregir únicamente la instrumentación temporal del benchmark para tomar la referencia temporal después de leer el timestamp del estado, por ejemplo:

```cpp
const uint32_t lastScanMs = io->last_scan_ms;
const uint32_t nowMs = millis();
const uint32_t age = (uint32_t)(nowMs - lastScanMs);
```

Con ello, si el runtime actualiza `last_scan_ms` antes de la copia, `nowMs` se toma después y no aparece el underflow artificial de -1 ms.

Como defensa adicional de diagnóstico, puede tratarse explícitamente un timestamp aparentemente futuro por una diferencia mínima como una carrera de snapshot y no como stale.

## Próximo gate

Repetir A14.4-G2 con el mismo perfil físico, corrigiendo únicamente el cálculo diagnóstico de edad I/O en el firmware temporal del DUT.

Objetivo:

- confirmar TCP 500 req/s limpio;
- confirmar RTU FC03/16 a ~50 Hz limpio;
- confirmar `IO_STALE=0` con la instrumentación corregida;
- confirmar full runtime limpio;
- no introducir todavía cambios de producción.
