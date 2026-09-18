# Alpha14 A14.3 — Commit del fix SD con handle único

Fecha: 2026-09-13

## Estado

El fix del harness `FULL_RUNTIME_REALISTIC` para evitar dos handles simultáneos sobre `/A14S2.LOG` quedó consolidado y publicado.

```text
HARNESS_FIX_COMMIT=bb166325410cdea765c8c6bd25ce641e77354a44
COMMIT_SCOPE=PASS
PUSH=PASS
WORKTREE_AFTER=PASS
DEVICE_FIRMWARE_MATCHES_COMMIT=YES
REFLASH_REQUIRED_FOR_NEXT_GATE=NO
DEVICE_RESET_REQUIRED=NO
```

## Cambio consolidado

El harness mantiene el archivo de append abierto durante la operación normal, pero durante `serviceSdVerify()`:

1. hace `flush()` del lote pendiente si corresponde;
2. cierra temporalmente el handle persistente `FILE_APPEND`;
3. abre un único handle `FILE_READ` para verificar el último registro;
4. cierra el handle de lectura;
5. reabre `FILE_APPEND` para continuar la operación normal.

Esto elimina la coexistencia simultánea de dos handles sobre el mismo archivo.

## Evidencia física previa al commit

La validación física a 100 req/s durante 60 s obtuvo:

```text
A14_3_FULL_RUNTIME_REALISTIC_AUTOMATED_SMOKE=PASS_PHYSICAL
SD_VERIFY_SINGLE_HANDLE=PASS_PHYSICAL
SD_VERIFY_CYCLES=12
SD_VERIFY_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
TCP_ERRORS=0
TCP_ACHIEVED_PCT=100.000
```

La corrida anterior con dos handles simultáneos había registrado 11 fallos de verify en 12 ciclos, por lo que la evidencia respalda la hipótesis de incoherencia entre handles FAT abiertos simultáneamente sobre el mismo archivo.

## Pendiente obligatorio

El resultado de 100 req/s valida corrección funcional, pero no sustituye la regresión de rendimiento final.

```text
FINAL_FULL_RUNTIME_1000RPS_60S=RETRY_PENDING
FINAL_FULL_RUNTIME_1000RPS_30MIN=ON_HOLD
```

El siguiente gate debe repetir `FULL_RUNTIME_REALISTIC` con FC03/125 a 1000 req/s durante 60 s, sin reflash ni reset, usando el firmware ya cargado correspondiente a `bb166325410cdea765c8c6bd25ce641e77354a44`.
