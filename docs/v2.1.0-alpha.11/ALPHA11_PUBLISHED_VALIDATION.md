# Alpha11 — Validación del package publicado

Fecha: 2026-09-09.

## Objetivo

Demostrar que `v2.1.0-alpha.11` funciona desde el artefacto realmente publicado y no desde el checkout local `jwplc_local`.

## Fuente

Índice utilizado:

```text
JWPLC/package_jwplc_index_dev.json
```

Versión instalada:

```text
jwplc:esp32@2.1.0-alpha.11
```

Entorno Arduino CLI aislado:

```text
%TEMP%\jwplc-alpha11-published-gate
```

La instalación efectiva quedó bajo:

```text
%TEMP%\jwplc-alpha11-published-gate\data\packages\jwplc\hardware\esp32\2.1.0-alpha.11
```

## Artefacto publicado

```text
TAG=v2.1.0-alpha.11
PUBLISHED_PACKAGE_SOURCE_SHA=64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
ZIP=jwplc-esp32-2.1.0-alpha.11.zip
SIZE=24547524
SHA256=465440cf92491b3c9050aa44b6184afae7bfa8e52993d6bc777487d203a97474
PACKAGE_ROOT=2.1.0/
```

## Paridad de archives

Core publicado:

```text
CORE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
PUBLISHED_CORE_PARITY=PASS
```

Display publicado:

```text
DISPLAY_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
PUBLISHED_DISPLAY_PARITY=PASS
```

Los dos hashes coinciden con los archives previamente validados durante el cierre técnico Alpha11.

## Compilación

Se compiló con:

```text
FQBN=jwplc:esp32:jwplcbasic
PLATFORM=jwplc:esp32@2.1.0-alpha.11
```

Resultado:

```text
ALPHA11_PUBLISHED_INSTALL=PASS
ALPHA11_PUBLISHED_ARCHIVE_PARITY=PASS
ALPHA11_PUBLISHED_COMPILE=PASS
```

## Upload y runtime físico

Puerto usado durante el gate:

```text
COM14
```

El sketch de regresión ejecutó continuamente:

```cpp
void loop()
{
    uint32_t dt = millis() - t0;
    bool q0 = ((dt % intervaloq0) * 2 < intervaloq0);
    digitalWrite(Q0_0, q0);
}
```

Sin `delay(1)`.

Observación física confirmada:

- TFT inicializa y permanece operativa;
- RTC continúa actualizándose;
- `Q0_0` conmuta continuamente;
- Display no se congela;
- no se observan resets inesperados;
- no se requiere `delay(1)`.

Resultado:

```text
ALPHA11_PUBLISHED_UPLOAD=PASS
ALPHA11_PUBLISHED_RUNTIME=PASS
ALPHA11_PUBLISHED_NO_DELAY_GATE=PASS
```

## Conclusión

El package publicado reproduce los archives y el comportamiento físicamente validados durante el desarrollo de Alpha11.

```text
ALPHA11_PUBLISHED_PACKAGE_GATE=PASS
```
