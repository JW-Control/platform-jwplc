# Alpha13 — cierre TFT-CLOSURE — 2026-10-09

## Decisión

`TFT_CLOSURE=CLOSED_PASS`, cierre físico validado con archive productivo y observación del operador. Este cierre no publica ni fusiona Alpha13 a release.

## Fuente de evidencia

- Ejecución: `20261009_220034_167fa2ad`; archivo original: `tools/alpha13/results/tft_closure_20261009_220034_167fa2ad/SUMMARY.log` y `MANIFEST.json` (resultados locales ignorados por Git).
- Evidencia versionada: [A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md](A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md).
- FQBN: `jwplc_local:esp32:jwplcbasic`; USB: `COM4`, VID:PID `1A86:7523`.
- Archive TFT productivo SHA-256: `ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8`.
- Binario del sketch subido SHA-256: `a0a173b2f86957a131ebb03626cf0800f2b54ef2fdf4b38ab138b5586440d910`.
- Token de ejecución reportado por serial en ambos arranques: `20261009_220034_167fa2ad_371a468311ab48b39392f4e59937f083`.

| Criterio | Resultado |
|---|---|
| Preflight, toolchain, COM | PASS |
| Compilación normal con `JWPLC_TFT.a` de producto | PASS |
| Upload físico y serial inicial | PASS |
| Reinicio USB-only y serial con misma procedencia | PASS |
| `DISPLAY_READY=YES` / `IO_READY=YES` | PASS |
| Fondo blanco irregular persistente/flicker | NO (operador) |
| TFT IDLE estable, sin boot loop | SÍ (operador) |
| Compilaciones de regresión Display/consumers | 4/4 PASS |
| Reconstrucción source-first y consumo precompiled normal | PASS |
| Paridad de miembros del archive regenerado | PASS |
| Video revisado directamente por asistente | NO, no suministrado |
| Reproducibilidad de hash binario del archive | NO; funcional sí |

```text
PRECOMPILED_PRODUCT_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
REBUILT_SHA256=44916b892fb30988ecdb25a2abef7d8d8b3d4f3f68c9a7a6bbc88ca55e35841d
REBUILT_ARCHIVE_BYTES=1092058
SOURCE_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
SOURCE_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
```

**Limitación:** el usuario constató tiempos percibidos comparables a la TFT previamente operativa; no se realizó A/B cronometrado. La observación visual del operador no se presenta como revisión de video.

## Trazabilidad Git

```text
TOOLING_COMMIT=b85cb9c37cde39c33b56c56fd7ef325ca6670355
PRODUCT_COMMIT=e211ff6acc85b57f97bf7a9d28fbb91025cd0bae
SYNC_MERGE_COMMIT=dbcc68019590cc5a37c2042b09db5ef6ac131b77
PRODUCT_STAGE_PATH_COUNT=3
REMOTE_PULL_RESET_CHECKOUT_CLEAN=NOT_EXECUTED
MERGE_TO_RELEASE=NOT_EXECUTED
GITHUB_PR_OR_PRE_RELEASE=NOT_EXECUTED
```

El commit productivo cambia sólo `JWPLC_TFT.cpp`, `tft_setup.h` y `libJWPLC_TFT.a`. El ejecutor y la receta de reconstrucción residen en `tools/alpha13/gates/a13_tft_closure.py` y `run_a13_tft_closure.bat`. Se mantuvo autoload normal, API pública, core.a y Display.a.

## Siguiente objetivo

`NEXT_GATE=A13-G3-TCA_RMW_SHADOW_ATOMICITY`, iniciando por revisión/plan de G3. No repetir PRE5/P0/P1A/P1B/P2A/P2B sin evidencia nueva. Exclusiones: OpenPLC, HMI Designer y nuevas funcionalidades TFT.
