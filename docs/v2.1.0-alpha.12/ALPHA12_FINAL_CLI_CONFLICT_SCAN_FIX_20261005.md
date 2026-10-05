# Alpha12 — Corrección del gate final CLI: scan de conflictos

Fecha: 2026-10-05

## Clasificación

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
SOURCE_FAILURE=NO
PRECOMPILED_FAILURE=NO
FALSE_CONFLICT_MARKERS=YES
```

El primer runner final CLI usó:

```text
^(<<<<<<< |=======|>>>>>>> )
```

Ese patrón trataba cualquier línea que comenzara con siete signos `=` como
marcador de conflicto. Por ello detectó separadores decorativos válidos en
README, ejemplos y logs históricos.

Además, el scan recorría todo el repositorio, incluyendo `JWPLC/Old` y
resultados históricos que no forman parte del scope Alpha12.

## Corrección

El gate final ahora usa dos verificaciones:

1. `git ls-files -u` como fuente autoritativa para conflictos no resueltos en
   el index.
2. Scan textual defensivo limitado a:
   - `JWPLC/2.1.0`
   - `docs/v2.1.0-alpha.12`
   - `tools/alpha12`

El patrón textual exige marcadores exactos:

```text
^<<<<<<< .+$
^=======$
^>>>>>>> .+$
```

Con ello no se confunden separadores como `====================`.

## Impacto

No hubo cambio bajo `JWPLC/2.1.0/`, archives ni policy de precompilación.

```text
SOURCE_FINAL_FREEZE=UNCHANGED
P7_POST_P8_REVALIDATION=STILL_VALID
FINAL_BUILD_SPEED_BENCHMARK_POST_P8=STILL_VALID
NEXT=RERUN_FINAL_ARDUINO_CLI_GATE
```


## Corrección de segundo orden

El primer intento de corregir el scan textual introdujo una corrupción del
propio runner durante la escritura del archivo:

```text
FIRST_FIX_RESULT=HARNESS_PARSE_FAILURE
PRODUCT_CODE_EXECUTED=NO
PACKAGE_SOURCE_TOUCHED=NO
```

La versión dañada quedó con el comando de `git grep` truncado y una sección
posterior duplicada. Se restauró el runner desde el commit sano anterior
`057454248dd66b339bd190e061e37ad8172da9ec` y se reaplicó únicamente la
corrección del scan.

La versión restaurada evita contener secuencias literales de merge y construye
los prefijos dinámicamente:

```powershell
$leftMarker = ("<" * 7) + " "
$rightMarker = (">" * 7) + " "
```

Verificación post-write:

```text
RUNNER_HEADER_COUNT=1
RUNNER_FINAL_PASS_COUNT=1
UNMERGED_INDEX_SCAN=PRESENT
DYNAMIC_LEFT_MARKER=PRESENT
DYNAMIC_RIGHT_MARKER=PRESENT
```

Commit de corrección:

```text
50434822d3bc987832a773f803c8731368693098
```

Clasificación final:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
SOURCE_FAILURE=NO
PRECOMPILED_FAILURE=NO
SOURCE_FINAL_FREEZE=UNCHANGED
```


## Tercera corrección: scope productivo

El runner restaurado parseó correctamente y confirmó:

```text
UNMERGED_INDEX_COUNT=0
```

pero el scan textual aún incluyó `docs/v2.1.0-alpha.12`. Como este mismo
documento reproduce ejemplos de marcadores de merge, el gate se auto-detectó y
reportó cuatro coincidencias documentales.

Clasificación:

```text
HARNESS_FAILURE=YES
SELF_MATCHING_AUDIT_SCOPE=YES
PRODUCT_FAILURE=NO
SOURCE_FAILURE=NO
```

La comprobación textual defensiva queda limitada exclusivamente al package
productivo actual:

```text
JWPLC/2.1.0
```

La comprobación global autoritativa de conflictos Git sigue siendo:

```text
git ls-files -u
```

De este modo, `docs/` y `tools/` pueden registrar marcadores de conflicto
como evidencia sin bloquear falsamente el release.

Commit del fix:

```text
d58b93c86d609770252012ec478fea8edc6b9306
```

Estado:

```text
SOURCE_FINAL_FREEZE=UNCHANGED
P7_POST_P8_REVALIDATION=STILL_VALID
FINAL_BUILD_SPEED_BENCHMARK_POST_P8=STILL_VALID
NEXT=RERUN_FINAL_ARDUINO_CLI_GATE
```
