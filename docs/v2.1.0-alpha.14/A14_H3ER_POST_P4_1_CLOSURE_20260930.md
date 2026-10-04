# Alpha14 — cierre H3E-R post-P4.1 — 2026-09-30

## Decisión

```text
H3E_R_POST_P4_1=PASS
PHYSICAL_RERUN_REQUIRED=NO
```

La ejecución física conservada en
`tools/modbus-tcp-benchmark/results/20260930_181806_h3er_post_p4_1/`
es válida. El error final del wrapper ocurrió después de que el gate físico y
la captura de artefactos terminaran correctamente.

## Clasificación explícita

| Clase | Resultado | Evidencia |
|---|---|---|
| `HARNESS_FAILURE` | **SÍ, CORREGIDO** | El parser buscaba solamente el texto literal `libjwplc_modbusrtu.a`; Arduino enlazó correctamente mediante `-L.../JWPLC_ModbusRTU/src/esp32 -lJWPLC_ModbusRTU`. |
| `PRODUCT_FAILURE` | **NO** | TCP, RTU, DataLog, periféricos y TFT cumplieron el gate. |
| `HARDWARE_FAILURE` | **NO** | Master/Slave, enlace Ethernet, RTU y TFT física completaron la prueba. |
| `ENVIRONMENT_FAILURE` | **NO** | El toolchain encontró y enlazó el archive cualificado en Master y Slave. |

## Causa raíz del falso negativo

El wrapper normalizaba el compile log y exigía:

```text
libjwplc_modbusrtu.a
```

Sin embargo, el comando real del linker en ambos builds usó la forma estándar
de GCC:

```text
-L.../libraries/JWPLC_ModbusRTU/src/esp32 -lJWPLC_ModbusRTU
```

`-lJWPLC_ModbusRTU` ordena al linker resolver
`libJWPLC_ModbusRTU.a` dentro del directorio pasado con `-L`. Por tanto, la
ausencia del nombre expandido en el texto del comando no significaba ausencia
del archive.

La corrección acepta dos formas equivalentes y verificables:

1. ruta directa terminada en `libJWPLC_ModbusRTU.a`;
2. directorio cualificado `JWPLC_ModbusRTU/src/esp32` junto con el flag
   `-lJWPLC_ModbusRTU`.

No se acepta un `-lJWPLC_ModbusRTU` aislado sin evidencia del directorio
cualificado.

## Revalidación offline

Se añadió:

```text
tools/modbus-tcp-benchmark/gates/a14_h3er_existing_run_revalidation.ps1
```

El revalidador sólo lee los artefactos guardados; no compila, no flashea, no
abre serial y no toca hardware.

Resultado:

```text
H3ER_TCP_ACHIEVED_REQ_S=1000.00
H3ER_TCP_ACHIEVED_PCT=100.000
H3ER_TCP_P95_US=1270.8
H3ER_TCP_P99_US=3666.0
H3ER_TCP_MAX_US=22676.2
H3ER_RTU_ACHIEVED_HZ=50.004
H3ER_RTU_SUCCESS=6001/6001
H3ER_DATALOG_FAILED_COMMITS=0
H3ER_PERIPHERAL_FAILURE_COUNT=0
H3ER_TFT_PHYSICAL=PASS
H3ER_MASTER_MODBUS_RTU_ARCHIVE_LINK_FORM=GCC_LIBRARY_FLAG
H3ER_SLAVE_MODBUS_RTU_ARCHIVE_LINK_FORM=GCC_LIBRARY_FLAG
H3ER_HARNESS_FAILURE=FIXED
H3ER_PRODUCT_FAILURE=NO
H3ER_HARDWARE_FAILURE=NO
H3ER_ENVIRONMENT_FAILURE=NO
H3E_R_POST_P4_1=PASS
H3ER_PHYSICAL_RERUN_REQUIRED=NO
```

## Contratos conservados

La revalidación también confirmó:

- W5500 a 26 MHz por el gate físico original;
- FIFO_REUSE y DLEN_REUSE tomados de los defaults del package, sin override;
- RTU `ASYNC`, TX `QUEUED`, RX `BYTE`, CRC `BITWISE`, framing `GAP`;
- HMI `HMI_ON_DEMAND_DIRTY` con `USER_REFRESH_ON_DEMAND`;
- Display, TFT, SPI, DataLog, Modbus TCP y Ethernet desde source;
- Modbus RTU desde el archive cualificado histórico;
- ausencia de archives stale de Display, TFT y SPI en el enlace.

## Cierre

```text
H3ER_GATE_STATUS=PASS
H3ER_FAILURE_DOMAIN=HARNESS_ONLY
H3ER_EXISTING_PHYSICAL_EVIDENCE=ACCEPTED
NEXT=MODBUS_RTU_SOURCE_FIRST_DEVELOPMENT
```
