# Alpha13 — Benchmark local Windows validado y cerrado (21/10/2026)

**Dictamen:** `LOCAL_PC_BENCHMARK=PASS_LOCAL_PC_THREE_PAIRS_TWO_FQBN`. Evidencia aportada por el operador desde su PC Windows, contrastada con la salida de PowerShell y conservada como [MANIFEST original](evidence/A13_LOCAL_PC_BENCHMARK_MANIFEST_20261021.json).

**Origen y reproducibilidad:** `HEAD=158d12a242aa09cf414207c10217954a034a0b68`, package tree `6b498f4a0797e2a1bb0054b005b91f985bc071e2`, tree funcional N3 previo `cc17716f173509b1e0f3c0df5eb99549014c1d61`, Arduino CLI `arduino-cli  Version: 1.5.1 Commit: 01f3d4f2b Date: 2026-06-05T10:22:12Z`. Solo se modificaron en el tree los README `JWPLC_Ethernet/README.md` y `JWPLC_ModbusTCP/README.md`; ningún `.cpp`, `.h`, `.a` ni `library.properties` productivo. El benchmark leyó las bibliotecas mediante **copias aisladas** y no cambió el package instalado. `hardware=NOT_TOUCHED`, upload **no medido**.

## Resultado definitivo en esta PC

Tres repeticiones por cada modo/FQBN. Los números de esta tabla son **medianas**, no el mejor intento, expresadas en segundos.

| Perfil | Fría CPP | Fría .a | Ahorro frío | Caliente CPP | Caliente .a | Diferencia caliente |
|---|---:|---:|---:|---:|---:|---:|
| JWPLC Basic | 52,120 s | 40,190 s | **11,930 s / 22,89 %** | 9,251 s | 9,055 s | 0,196 s / 2,12 % |
| JWPLC Basic Core | 54,964 s | 47,958 s | **7,006 s / 12,75 %** | 9,175 s | 9,146 s | 0,029 s / 0,32 % |

**Lectura para el usuario:** en esta PC la primera compilación de JWPLC Basic bajó de **52,120 s a 40,190 s (22,89 % menos)**, y Basic Core de **54,964 s a 47,958 s (12,75 % menos)**. Las recompilaciones sin modificar el sketch tardaron cerca de 9 s en ambos modos; la pequeña diferencia caliente no demuestra una mejora robusta en otros equipos.

## Todas las muestras de la campaña

| Perfil | Bibliotecas | Frías 1 / 2 / 3 (s) | Calientes 1 / 2 / 3 (s) | Objetos CPP Ethernet / Modbus |
|---|---|---|---|---|
| JWPLC Basic | Fuente CPP | 57,890 / 52,120 / 50,747 | 9,425 / 9,181 / 9,251 | 12 / 2 |
| JWPLC Basic | Archive .a | 40,190 / 40,018 / 41,408 | 9,120 / 9,055 / 8,942 | 0 / 0 |
| JWPLC Basic Core | Fuente CPP | 59,470 / 54,489 / 54,964 | 9,133 / 9,207 / 9,175 | 12 / 2 |
| JWPLC Basic Core | Archive .a | 47,958 / 48,086 / 47,624 | 9,146 / 9,192 / 8,897 | 0 / 0 |

**Prueba de procedencia:** cada repetición fuente generó `JWPLC_Ethernet=12` y `JWPLC_ModbusTCP=2` objetos `.cpp.o`; cada repetición archive generó **0 / 0**. Los 12 runs terminaron `PASS`, resultando 12 tiempos fríos y 12 calientes. Las huellas SHA-256 de los archivos aprobados coinciden:

| Archivo | SHA-256 verificado |
|---|---|
| `libJWPLC_Ethernet.a` | `f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4` |
| `libJWPLC_ModbusTCP.a` | `d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d` |

**Archivo de evidencia:** `docs/v2.1.0-alpha.13/evidence/A13_LOCAL_PC_BENCHMARK_MANIFEST_20261021.json`, formato original Windows CRLF, SHA-256 del archivo adjuntado `6bdf6c969f5bf776c648225a901402f6fec298f199fdaa259a87f436b76ebb59`. Se conservaron también los doce SHA-256 ELF y métricas de cada muestra. Los logs completos permanecen en el `RESULT_ROOT` temporal del operador y no son necesarios para atribuir PASS a estas mediciones.

## Comparación con CI: distinto equipo y distinta muestra

| Entorno | Basic, ahorro frío | Basic Core, ahorro frío |
|---|---:|---:|
| GitHub Actions Ubuntu (F137) | 30,4 % | 25,4 % |
| GitHub Actions Windows (F137) | 39,0 % | 15,9 % |
| **PC Windows del operador** | **22,89 %** | **12,75 %** |

Los tres entornos realizaron mediciones propias. **No** trasladar 85,163 s de Windows CI a la PC del operador ni combinar muestras de máquinas distintas para generar una media universal.

## Alcance de validez y límites

- El comparativo mide **compilación**, no instalación por Boards Manager ni tiempo de transferencia USB, flashing, reset o arranque.
- El modo `source` elimina los dos archivos `.a` y el flag `precompiled=full` **solo de la copia temporal**, para que el compilador regenere esos catorce objetos. El modo `archive` conserva los .a oficiales sin volver a compilar sus CPP.
- No se alteraron autoload, pantalla, Modbus RTU, RS-485, FRAM, SD, RTC, botonera, TCA/I/O, pines, `platform.txt`, `boards.txt` ni bootloader.
- Esta campaña mide un sketch consumidor representativo en una PC. No garantiza una reducción porcentual idéntica en todos los sketches o en otras máquinas. Hubo tres pasadas por modo en secuencia; los primeros builds pueden sufrir calentamiento de caché y ruido del host. Se reporta **mediana**, no mínimo.
- El tree del package incorporó solamente mejoras documentales posteriores al N3 original, por lo que su SHA completo es distinto sin una recompilación de los dos archives. La CI release-like valida el contenido del ZIP actual y ambos FQBN.
- `UPLOAD_BENCHMARK=NOT_MEASURED`; `APP_ONLY` y la decisión sobre bootloader precompilado no se modifican por este ensayo.
- Las pruebas físicas previas son acotadas. R3 interno, R4 de 1805 s, DHCP/DNS específico, Basic Core físico y E1/E2 siguen como límites aceptados; este benchmark no los cubre.

## Conclusión

```text
LOCAL_PC_BENCHMARK=PASS_LOCAL_PC_THREE_PAIRS_TWO_FQBN
WINDOWS_PC_BASIC_COLD_SAVINGS=11.930_S_22.89_PERCENT
WINDOWS_PC_BASICCORE_COLD_SAVINGS=7.006_S_12.75_PERCENT
PRECOMPILED_ARCHIVES=PASS_HASHES_UNCHANGED
AUTOLOAD_PERIPHERALS_REMOVED=NO
USB_UPLOAD_BENCHMARK=NOT_MEASURED
SOURCE_PACKAGE_FUNCTIONAL_DIFF=EMPTY
BENCHMARK_CLOSE=PASS_NONPHYSICAL
ALPHA13=OPEN_NOT_PUBLISHED
```

Con el benchmark documentado, el **README de Alpha13 puede cerrarse técnicamente**, sujeto a CI del HEAD documental final y revisión/merge autorizado por el operador. El estado de publicación y el gate de instalación real desde índice dev permanecen separados.
