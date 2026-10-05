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
