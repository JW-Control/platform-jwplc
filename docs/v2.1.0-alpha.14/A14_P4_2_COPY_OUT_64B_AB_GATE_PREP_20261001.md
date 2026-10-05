# Alpha14 — P4.2 COPY_OUT 64 B — preparación gate A/B 2026-10-01

## Estado de entrada

~~~text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
HEAD=3852891106e2d1c8a82b15287f1091ae23a0ca96
P4_2_PLAN=PASS
P4_2_SOURCE_CONTRACT=PASS
P4_2_DEFAULT_BUILD=PASS
P4_2_CANDIDATE_ON_COMPILE=PASS
P4_2_DEFAULT_OFF=PASS
P4_2_PHYSICAL_AB=NOT_RUN
P4_2_PROMOTION=NOT_DECIDED
~~~

~~~text
P4_2_AB_HARNESS_READY=PASS
P4_2_ONLY_VARIABLE=COPY_OUT_64_POLICY
P4_2_DEFAULT_OFF=PASS
P4_2_PHYSICAL_AB=NOT_RUN
P4_2_PROMOTION=NOT_DECIDED
~~~

Este documento sólo prepara el gate. No registra resultados físicos.

## Harness

~~~text
GATE=tools/modbus-tcp-benchmark/gates/a14_p4_2_w5500_copy_out_64_ab.py
ONLY_VARIABLE=COPY_OUT_64_POLICY
BASELINE=JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=0
CANDIDATE=JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1
CANDIDATE_DEFAULT=OFF
~~~

El harness reutiliza el firmware y el runner TCP RX de P4.1. Para cada modo
de build, los flags BASELINE/CANDIDATE difieren exclusivamente en
JWPLC_SPI_FIFO_REUSE_COPY_OUT_64. El gate no sobreescribe
JWPLC_SPI_FIFO_REUSE_DLEN_CACHE: verifica que su default promovido siga en
1.

Constantes de ambas variantes:

~~~text
W5500_SPI_HZ=26000000
FIFO_REUSE=ON_DEFAULT
DLEN_REUSE=ON_DEFAULT
DIRECT_RX=OFF
RX_COMMIT=IMMEDIATE
LEGACY_APIS=UNCHANGED
ETHERNET_STIMULUS=H4A04P1_TCP_RX_CASE
TCP_CHUNK=4096
PAYLOAD_PATTERN=INCREMENTING_00_FF
HARDWARE=UNCHANGED_FROM_P4_1
TOPOLOGY=UNCHANGED_FROM_P4_1
~~~

El script audita los defaults, la frecuencia, el bloque candidato, las 16
copias explícitas, la ausencia de memcpy sobre MMIO y la preservación del
tail antes de buscar Arduino CLI.

## Secuencia heredada de P4.1

1. FNV del candidato durante 1.0 s.
2. Microperfil BASELINE y CANDIDATE durante 3.0 s por variante.
3. A/B sin profiler durante 15.0 s por run.
4. Revisión física explícita.
5. Clasificación sin promoción automática.

Modo inicial:

~~~text
RUNS_PER_VARIANT=3
ORDER=BASELINE,CANDIDATE,CANDIDATE,BASELINE,BASELINE,CANDIDATE
PERF_DURATION_S=15.0
~~~

Confirmación opcional para efecto pequeño/inconcluso:

~~~text
ARGUMENT=--confirmation
RUNS_PER_VARIANT=5
ORDER=BASELINE,CANDIDATE,CANDIDATE,BASELINE,BASELINE,CANDIDATE,CANDIDATE,BASELINE,BASELINE,CANDIDATE
PERF_DURATION_S=15.0
~~~

## Métricas

Integridad:

~~~text
FNV_ACTUAL
FNV_EXPECTED
PAYLOAD_INTEGRITY
TRANSPORT_ERRORS
TCP_SPI_LOCK_ERRORS
UNEXPECTED_RESETS
~~~

Cada caso válido termina con ACK de freeze y snapshot serial final. Un reboot
durante la ventana invalida el caso; sólo después de completar todos los casos
se reporta UNEXPECTED_RESETS=0.

Microperfil:

~~~text
COPY_OUT_US_PER_64B_CHUNK_BASELINE
COPY_OUT_US_PER_64B_CHUNK_CANDIDATE
COPY_OUT_64B_DELTA_PCT
~~~

El profiler existente entrega tiempo total de copy-out y bytes. El coste por
64 B se normaliza como:

~~~text
COPY_OUT_US_PER_64B_CHUNK = COPY_OUT_TOTAL_US / (BYTES / 64)
~~~

El patrón, duración e instrumentación son iguales para ambas variantes.

Sistema:

~~~text
BASELINE_PAYLOAD_MBPS
CANDIDATE_PAYLOAD_MBPS
PAYLOAD_DELTA_PCT

BASELINE_US_PER_BYTE
CANDIDATE_US_PER_BYTE
US_PER_BYTE_DELTA_PCT

BASELINE_TCP_MBPS
CANDIDATE_TCP_MBPS
TCP_DELTA_PCT
~~~

TCP end-to-end se conserva como métrica secundaria porque P4.1 registró mayor
dispersión allí que en payload interno.

Repetibilidad:

~~~text
BASELINE_PAYLOAD_SPREAD_PCT
CANDIDATE_PAYLOAD_SPREAD_PCT
BASELINE_TCP_SPREAD_PCT
CANDIDATE_TCP_SPREAD_PCT
BASELINE_US_PER_BYTE_SPREAD_PCT
CANDIDATE_US_PER_BYTE_SPREAD_PCT
BASELINE_VALID_RUNS
CANDIDATE_VALID_RUNS
~~~

## Criterios

Integridad obligatoria:

~~~text
FNV_ACTUAL=FNV_EXPECTED
PAYLOAD_INTEGRITY=PASS
TRANSPORT_ERRORS=0
TCP_SPI_LOCK_ERRORS=0
UNEXPECTED_RESETS=0
~~~

Repetibilidad heredada de P4.1:

~~~text
PAYLOAD_SPREAD_MAX_PCT=0.75
VALID_RUNS_PER_VARIANT=REQUESTED_RUNS_PER_VARIANT
~~~

Evidencia del mecanismo definida por el plan P4.2:

~~~text
COPY_OUT_64B_DELTA_PCT <= -2.0
~~~

Ganancia de sistema:

~~~text
PAYLOAD_DELTA_PCT >= +0.25
AND
US_PER_BYTE_DELTA_PCT <= -0.25
~~~

Regresión material:

~~~text
PAYLOAD_DELTA_PCT <= -0.50
OR
US_PER_BYTE_DELTA_PCT >= +0.50
~~~

Una mejora sólo en microperfil no promueve el candidato. Las clasificaciones
son:

~~~text
P4_2_GAIN_CONFIRMED
P4_2_NO_MATERIAL_SYSTEM_GAIN
P4_2_SMALL_OR_INCONCLUSIVE_EFFECT
P4_2_REPEATABILITY_INSUFFICIENT
P4_2_REGRESSION
P4_2_INTEGRITY_FAIL
~~~

## Validación offline

~~~text
P4_2_GATE_AST=PASS
P4_2_ONLY_VARIANT_FLAG=JWPLC_SPI_FIFO_REUSE_COPY_OUT_64
P4_2_BASELINE_FLAG=JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=0
P4_2_CANDIDATE_FLAG=JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1
P4_2_DLEN_REUSE_BUILD_OVERRIDE=NO
P4_2_DEFAULT_OFF=PASS
P4_2_PHYSICAL_AB=NOT_RUN
P4_2_PROMOTION=NOT_DECIDED
~~~
