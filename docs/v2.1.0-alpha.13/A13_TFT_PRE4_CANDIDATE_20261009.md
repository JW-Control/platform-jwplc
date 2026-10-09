# Alpha13 — TFT-PRE4 — Candidato temporal deferred-DISPON

Fecha: 2026-10-09

## Resultado

```text
GATE=A13-TFT-PRE4
STATUS=PASS
REASON=TEMP_DEFERRED_DISPON_CANDIDATE_READY
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

## Identidad del candidato

```text
CANDIDATE_JWPLC_TFT_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
CANDIDATE_TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
CANDIDATE_ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
```

## Contrato

```text
BACKEND_ORIGINAL_MUTATED=NO
REPO_PRODUCT_MUTATED=NO
DISPON_DEFERRED_BY_PRIVATE_MACRO=YES
GRAM_BLACK_BEFORE_DISPON=YES
POST_DISPON_DELAY_MS=120
WORKTREE_FINAL=CLEAN
DIFF_CHECK_FINAL=True
```

El candidato no reduce los delays conservadores del ST7789. Mueve la
visibilidad del panel: el backend termina la configuración con display OFF,
`JWPLC_TFT` limpia GRAM a negro y recién después envía DISPON y conserva el
settle de 120 ms.

## Siguiente paso

TFT-PRE5 compilará source-first desde una copia temporal del candidato,
forzando el setup privado de forma reproducible, subirá el firmware de
baseline al JWPLC y dejará el equipo listo para comparación visual por
power-cycle. El repositorio y la instalación global TFT_eSPI deben permanecer
sin modificaciones.
