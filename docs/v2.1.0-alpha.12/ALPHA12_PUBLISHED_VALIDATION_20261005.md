# Alpha12 — Validación del package publicado

Fecha: 2026-10-05

## Objetivo

Demostrar que `v2.1.0-alpha.12` funciona desde el artefacto realmente
publicado y desde el índice dev de `main`, sin reutilizar `jwplc_local`.

## Publicación

```text
TAG=v2.1.0-alpha.12
PUBLISHED_PACKAGE_SOURCE_SHA=1011f2588fe02bdc67b14bef8c33ad3624426cb6
ZIP=jwplc-esp32-2.1.0-alpha.12.zip
SIZE=24383662
SHA256=412079a9e01cb0eaccdf6ec530b04183db1c245eb043e846fe9f0f7e2d5eb1b5
PACKAGE_ROOT=2.1.0/
```

## Índice e instalación aislada

Índice:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Entorno:

```text
%TEMP%\jwplc-alpha12-published-gate
FQBN=jwplc:esp32:jwplcbasic
VERSION=2.1.0-alpha.12
```

Resultado:

```text
ALPHA12_VERSION_INSTALLED=True
PACKAGE_EXISTS=True
ALPHA12_PUBLISHED_INDEX=PASS
ALPHA12_PUBLISHED_INSTALL=PASS
```

## Autocontención publicada

```text
BUNDLED_BUSIO_EXISTS=True
LEGACY_GFX_EXISTS=False
LEGACY_ST77XX_EXISTS=False
ALPHA12_PUBLISHED_AUTOCONTAINMENT=PASS
```

## Paridad de archives

```text
PUBLISHED_FRAM_SHA256=b8734763bfa1287167feda72a5c9b30df97340d41ca0c3fd000321e218632ceb
PUBLISHED_FRAM_PARITY=True

PUBLISHED_CORE_SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
PUBLISHED_CORE_PARITY=True

ALPHA12_PUBLISHED_ARCHIVE_PARITY=PASS
```

## Correcciones CI incluidas en el ZIP

```text
FLAPPY_COMPAT_HELPER_PRESENT=True
FLAPPY_RAW_TFT_TRIANGLE_PRESENT=False
ETH_ST77XX_RESIDUE_PRESENT=False
ALPHA12_PUBLISHED_CI_FIXES_PRESENT=PASS
```

## Compilación aislada

Sketch:

```text
JWPLC_Ethernet/examples/Ethernet_HTTP_TFT_Diagnostics
```

Resultado:

```text
COMPILE_EXIT=0
PUBLISHED_PLATFORM_SELECTED=True
PUBLISHED_BUNDLED_BUSIO_SELECTED=True
JWPLC_LOCAL_SELECTED=False
ALPHA12_PUBLISHED_COMPILE=PASS
```

## Upload físico post-publicación

Puerto:

```text
COM4
```

Resultado:

```text
COMPILE_UPLOAD_EXIT=0
PUBLISHED_PLATFORM_SELECTED=True
JWPLC_LOCAL_SELECTED=False
ALPHA12_PUBLISHED_UPLOAD=PASS
```

## Runtime post-upload

Heartbeat observado:

```text
ALPHA12_PUBLISHED_RUNTIME_HEARTBEAT DISPLAY_READY=1 RTC_OK=1 Q0_0=1
ALPHA12_PUBLISHED_RUNTIME_HEARTBEAT DISPLAY_READY=1 RTC_OK=1 Q0_0=0
ALPHA12_PUBLISHED_RUNTIME_HEARTBEAT DISPLAY_READY=1 RTC_OK=1 Q0_0=1
ALPHA12_PUBLISHED_RUNTIME_HEARTBEAT DISPLAY_READY=1 RTC_OK=1 Q0_0=0
```

Clasificación:

```text
POST_UPLOAD_BOOT=PASS
POST_UPLOAD_TFT_RUNTIME_READY=PASS
POST_UPLOAD_RTC=PASS
POST_UPLOAD_Q0_LOGICAL_TOGGLE=PASS
ALPHA12_PUBLISHED_RUNTIME=PASS
```

La conversación no registró una observación visual/audible separada del relé o
de la TFT; el gate final se apoya en el upload físico real y en el estado de
runtime reportado por el package publicado.

## Conclusión

```text
ALPHA12_PUBLISHED_PACKAGE_GATE=PASS
JWPLC_LOCAL_SELECTED=False
```
