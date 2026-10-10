# Alpha13 — TFT-CLOSURE — evidencia física verificada (2026-10-09)

**Alcance:** cierre de pruebas TFT-PRE6-P2B con el archive productivo normal de `JWPLC_TFT`; fuente de evidencia: `SUMMARY.log`, `MANIFEST.json` y confirmación directa del operador durante la ejecución.

## Identidad de la prueba

```text
HITO=TFT-CLOSURE
RUN_ID=20261009_220034_167fa2ad
HEAD_LOCAL=ac69e41922f512c280ad116bcf79b24596befa02
PORT=COM4
VID_PID=1A86:7523
FQBN=jwplc_local:esp32:jwplcbasic
TFT_PRODUCT_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
APP_BINARY_SHA256=a0a173b2f86957a131ebb03626cf0800f2b54ef2fdf4b38ab138b5586440d910
RUNTIME_TOKEN=20261009_220034_167fa2ad_371a468311ab48b39392f4e59937f083
```

## Evidencia de ejecución

| Verificación | Resultado |
|---|---|
| Preflight: 3 archivos TFT y SHA esperados | PASS |
| Arduino CLI y puerto físico CH340 | PASS |
| Compilación normal con archive TFT productivo | PASS |
| Upload al dispositivo | PASS |
| Serial inicial con token único y hash archive | PASS |
| Desconexión/reconexión USB-only | Confirmada por operador |
| Serial posterior al ciclo USB con mismo token/hash | PASS |
| `DISPLAY_READY=YES`, `IO_READY=YES` antes y después | PASS |
| TFT: fondo blanco irregular persistente/flicker | NO, informado por operador |
| TFT: IDLE limpia y sin boot-loop | SÍ, informado por operador |
| Regresiones compiladas: IDLE, HMI Fields, Idle Modes, Logic Runtime UI | 4/4 PASS |
| Reconstrucción del archive desde fuentes productivas parcheadas | PASS |
| Dos miembros extraídos/reconstruidos y paridad | PASS |
| Consumidor normal del archive reconstruido | PASS |

Telemetría inicial: `SETUP_ENTRY_MS=676`; tras reinicio USB: `SETUP_ENTRY_MS=696`. Estos valores son de entrada a setup, no representan un benchmark completo de tiempo de TFT visible.

Hashes de las tres modificaciones **locales, aún no versionadas al redactar esta evidencia**:

```text
JWPLC_TFT.cpp=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
tft_setup.h=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
libJWPLC_TFT.a=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
```

La reconstrucción temporal es funcional pero **no byte-idéntica** al archive adoptado:
```text
REBUILT_ARCHIVE_SHA256=44916b892fb30988ecdb25a2abef7d8d8b3d4f3f68c9a7a6bbc88ca55e35841d
REBUILT_ARCHIVE_BYTES=1092058
REBUILT_MEMBER_PARITY=PASS
```

## Clasificación y límites

```text
TFT_PRE6_P2B_PHYSICAL=PASS
VISUAL_OBSERVATION=PASS_OPERATOR_REPORTED
VIDEO_REVIEW_BY_ASSISTANT=NOT_PERFORMED
REBUILD_FUNCTIONAL=PASS
REBUILD_BIT_IDENTICAL=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

No presentar observación informada como video revisado. El usuario indicó que la TFT mantiene funcionamiento y tiempos percibidos comparables al estado bueno previo; no se realizó una medición cronometrada A/B.

**Pendiente al escribir este documento:** integrar el ejecutor en Git, versionar con commit exclusivo los 3 archivos TFT desde el equipo donde existen los bytes modificados, actualizar el estado canónico/checklist y avanzar a G3. No iniciar G3 antes del cierre documental de TFT. No merge a release ni publicación de Alpha13 por este gate.

Exclusiones: `OPENPLC`, `HMI_DESIGNER`, `TFT_NEW_FEATURES`.
