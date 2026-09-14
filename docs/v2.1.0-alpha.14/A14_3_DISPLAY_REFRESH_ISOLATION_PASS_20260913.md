# Alpha14 A14.3 — Aislamiento del refresco dinámico TFT

Fecha: 2026-09-13

## Resultado

```text
A14_3_DISPLAY_REFRESH_ISOLATION=PASS_PHYSICAL
DISPLAY_DYNAMIC_REFRESH_DOMINANT_MATERIAL=YES
FULL_RUNTIME_SD_OFF_TFT_REFRESH_OFF=100.000_PERCENT
NEXT=QUANTIFY_TFT_REFRESH_POLICY
```

## Objetivo

Aislar el impacto del refresco dinámico de la pantalla TFT dentro del perfil `FULL_RUNTIME_REALISTIC`, después de comprobar que:

- el baseline TCP-only actual reproduce ~100 %;
- el full runtime con workload SD completamente fuera del scheduler cae a ~89.8 %;
- el antiguo baseline de 95.993 % con SD workload OFF no se reprodujo bajo las condiciones actuales.

La variante D0e mantuvo:

- `JWPLC_Display` inicializado y `DISPLAY_READY=YES`;
- FRAM activa;
- RTC activo;
- TCA/I/O activo;
- botonera activa;
- probe SPI activo;
- Ethernet y Modbus TCP activos;
- SD inicializada y disponible;
- archivo SD persistente abierto;
- `serviceSdAppend()` fuera del scheduler;
- `serviceSdVerify()` fuera del scheduler;
- únicamente el callback de refresco dinámico USER de TFT devolviendo `false`.

No se removió ningún periférico del autoload normal.

## Evidencia física

Se ejecutaron dos corridas FC03/125 a 1000 req/s durante 60 s con el mismo firmware diagnóstico.

### Corrida 1

```text
ACHIEVED_REQ_S=1000.00
ACHIEVED_PCT=100.000
P95_US=1204.4
P99_US=1306.8
MAX_US=8304.2
LOOP_AVG_US=278
LOOP_MAX_US=1670
TCP_CLEAN=YES
RUNTIME_READY=YES
DISPLAY_FRAMES=0
DISPLAY_REFRESH_OFF_CONFIRMED=YES
SD_APPEND_CYCLES=0
SD_VERIFY_CYCLES=0
PERIPHERAL_FAILURE_COUNT=0
DIAGNOSTIC_CLEAN=YES
```

### Corrida 2

```text
ACHIEVED_REQ_S=1000.00
ACHIEVED_PCT=100.000
P95_US=1198.8
P99_US=1304.2
MAX_US=3085.8
LOOP_AVG_US=278
LOOP_MAX_US=1850
TCP_CLEAN=YES
RUNTIME_READY=YES
DISPLAY_FRAMES=0
DISPLAY_REFRESH_OFF_CONFIRMED=YES
SD_APPEND_CYCLES=0
SD_VERIFY_CYCLES=0
PERIPHERAL_FAILURE_COUNT=0
DIAGNOSTIC_CLEAN=YES
```

### Resumen

```text
AVG_PCT=100.000
MIN_PCT=100.000
MAX_PCT=100.000
SPREAD_PP=0.000
RECOVERY_VS_D0C_PP=10.157
REMAINING_GAP_TO_TCP_ONLY_PP=-0.008
ALL_DIAGNOSTIC_CONDITIONS_CLEAN=YES
```

## Comparación acumulada

| Perfil | Throughput |
|---|---:|
| TCP-only actual | 99.992 % |
| Full runtime + SD workload OFF | 89.843 % |
| Full runtime + SD workload OFF + TFT dynamic refresh OFF | 100.000 % |

El resultado recupera completamente el rendimiento TCP-only sin retirar Display del runtime, únicamente suprimiendo el redibujado dinámico periódico de la pantalla USER.

## Interpretación

El cuello de botella dominante de las pruebas full-runtime actuales no está en Ethernet, Modbus TCP, la PC, el runner ni en la mera presencia de la SD. La evidencia física identifica al refresco dinámico TFT del harness como contribución dominante a la pérdida de throughput observada.

El harness `FULL_RUNTIME_REALISTIC` configura actualmente `DISPLAY_PERIOD_MS=100`, por lo que fuerza un refresco USER cada 100 ms. Además, su callback actual redibuja varias regiones de la TFT en cada frame: tres líneas dinámicas y una barra gráfica. Este patrón es deliberadamente más pesado que un HMI basado en invalidación/dirty fields y sirve como estrés realista, pero demuestra que un refresco periódico agresivo puede competir materialmente por el SPI/tiempo de CPU con Modbus TCP.

Este gate no justifica deshabilitar Display ni retirar su autoload. La dirección correcta es cuantificar una política de refresco menos agresiva y/o favorecer refresco por cambios/dirty fields.

## API existente relevante

`JWPLC_Display` ya expone:

```cpp
JWPLC_Display.setUserRefreshPeriodMs(...);
JWPLC_Display.setUserRefreshMode(...);
JWPLC_Display.requestUserRefresh();
```

Por compatibilidad no se cambia todavía la API. Primero se cuantificará físicamente una cadencia viable.

## Siguiente gate

Evaluar un primer punto intermedio de `250 ms` para el refresco USER, manteniendo exactamente el resto de D0e y reactivando el callback dinámico.

Criterio inicial:

- `>=95 %` en ambas corridas: periodo 250 ms viable para seguir afinando hacia menor latencia visual;
- `93–95 %`: contribución material todavía presente; revisar 500 ms o estrategia por cambios;
- `<93 %`: el redibujado completo periódico sigue siendo demasiado costoso incluso a 4 Hz y se priorizará dirty/on-change.

```text
FINAL_FULL_RUNTIME_1000RPS_60S=ON_HOLD
FINAL_FULL_RUNTIME_1000RPS_30MIN=ON_HOLD
NEXT_GATE=D0F_TFT_REFRESH_250MS
```
