# Alpha14.3 — Refresh L2 productivo consolidado

Fecha: 2026-09-13

## Resultado

Se consolidó y publicó la implementación productiva de refresh L2 periódico en `JWPLC_Ethernet`.

Commit fuente:

`ea8ea87c2c9adc63970e6b1fe78b3dfe1296b93b`

Mensaje:

`fix(ethernet): mantener presencia L2 durante inactividad`

## Alcance exacto

El commit modifica únicamente:

- `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp`
- `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.h`

No se modificaron periféricos, autoload, Modbus TCP, Modbus RTU ni el harness de benchmark.

## Implementación

La mitigación productiva:

- ejecuta un refresh L2 best-effort cada `120000 ms` cuando Ethernet está READY;
- usa broadcast dirigido calculado a partir de IP y máscara;
- transmite un payload de 4 bytes (`JWL2`);
- usa socket UDP local efímero;
- no depende del gateway;
- omite el refresh mientras exista mantenimiento DHCP cooperativo en curso;
- no degrada `READY` ni sustituye el diagnóstico Ethernet si el refresh falla;
- permite desactivarse por compilación con `JWPLC_ETH_L2_REFRESH_PERIOD_MS=0`.

## Evidencia previa

Antes de consolidar el commit ya se validó:

- reproducción del fallo aged-reaccept tanto con DHCP como con IP estática;
- reconstrucción ARP inmediata tras borrar neighbor;
- rescate inmediato mediante una transmisión saliente del JWPLC;
- keepalive periódico hacia gateway durante 480 s;
- broadcast dirigido periódico durante 480 s;
- smoke físico del código productivo con full runtime, periféricos activos y FC03 operativo.

## Estado

```text
PRODUCTION_L2_REFRESH=COMMITTED
SOURCE_COMMIT=ea8ea87c2c9adc63970e6b1fe78b3dfe1296b93b
PRODUCTION_L2_REFRESH_PERIOD_MS=120000
PRODUCTION_L2_REFRESH_TARGET=DIRECTED_BROADCAST
PRODUCTION_L2_REFRESH_FAILURE_POLICY=BEST_EFFORT_NON_FATAL
PRODUCTION_L2_AGING_480S=PENDING
FINAL_1000RPS_60S=PENDING_MANDATORY
FINAL_1000RPS_30MIN=PENDING_MANDATORY
```

## Próximo gate

Ejecutar aging físico de 480 s usando exactamente el firmware ya cargado, sin recompilar ni reflashear. El objetivo es confirmar que la implementación productiva evita el outage de reaccept observado previamente tras inactividad prolongada.
