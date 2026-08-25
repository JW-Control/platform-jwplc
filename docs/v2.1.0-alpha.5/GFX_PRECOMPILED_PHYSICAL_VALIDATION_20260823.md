# Alpha5 - cierre del piloto precompilado Adafruit_GFX_Library

Fecha: 2026-08-23.

## Objetivo

Validar que el archive compartido de `Adafruit_GFX_Library` generado historicamente bajo `JWPLC_BASIC` puede permanecer precompilado sin romper:

- `ESP32 Board` generico;
- `JWPLC Basic`;
- `JWPLC Basic Core`;
- el funcionamiento fisico del TFT del JWPLC Basic.

Archive historico reutilizado de Alpha4:

```txt
Git blob: 0b8b9ad2f7ce449a485635c702e08017194fa204
```

No se regenero el archive ni se modifico la API publica de Adafruit GFX.

## Gate estatico bridge-compatible - PASS

La auditoria global con `-AllowGenericGpioBridge` encontro 9 archives y clasifico `Adafruit_GFX_Library` como:

```txt
[BRIDGE] Adafruit_GFX_Library/libAdafruit_GFX_Library.a: jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode
```

Los tres simbolos son exactamente los permitidos por el bridge GPIO generico. No aparecio ningun simbolo `jwplc_*` adicional bloqueante y el auditor termino con exit code 0.

## Gates cross-board - PASS

Sketch de enlace:

```txt
tools/build-speed-benchmark/sketches/09_gfx_bridge_link/09_gfx_bridge_link.ino
```

Resultados:

| Target | Resultado | App |
|---|---|---:|
| `jwplc_local:esp32:esp32` | PASS | 305720 B |
| `jwplc_local:esp32:jwplcbasic` | PASS | 406801 B |
| `jwplc_local:esp32:jwplcbasiccore` | PASS | 352788 B |

En los tres casos Arduino Builder consumio `Adafruit_GFX_Library` desde `src/esp32` como libreria precompilada y el linker incorporo `-lAdafruit_GFX_Library`. No se observaron `undefined reference to jwplc_*`.

### Observacion de reproducibilidad para BusIO

Durante el gate generico de GFX, Arduino resolvio `Adafruit_BusIO` desde el sketchbook del usuario, mientras que Basic y Basic Core usaron la copia vendorizada del package. Esto no invalida el gate GFX, porque el archive probado y forzado fue GFX, pero obliga a que el proximo piloto BusIO use el marker unico `JWPLC_Bundled_Adafruit_BusIO.h` y rechace cualquier seleccion del sketchbook.

## Gate fisico del display - PASS

Sketch:

```txt
tools/build-speed-benchmark/sketches/10_gfx_physical/10_gfx_physical.ino
```

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

El build/upload termino correctamente y el log confirmo que `Adafruit GFX Library` fue consumida como precompilada desde `src/esp32`.

Salida serial observada manualmente tras reiniciar el equipo desde el Serial Monitor de VS Code:

```txt
JWPLC Alpha5 - gate fisico Adafruit_GFX precompilada
JWPLC_Display inicializado
[GFX] Display ready: YES
[GFX] USER screen dibujada
[GFX] TFT size: 320x170
```

La validacion visual confirmo en hardware:

- marco blanco;
- diagonal roja;
- diagonal verde;
- circulo azul relleno;
- circulo amarillo sin relleno;
- triangulo cian relleno;
- texto `GFX`;
- texto `ALPHA5 PHYSICAL PASS`.

### Observacion visual no bloqueante

Las diagonales presentan interrupciones donde otras primitivas se dibujan posteriormente. El comportamiento corresponde al orden de renderizado y solapamiento del sketch de gate, no a una falla del archive precompilado. Las primitivas, colores, texto y geometria esperados fueron visibles correctamente.

### Observacion de tooling no bloqueante

La captura serial automatica del script PowerShell no alcanzo el mensaje de arranque. La evidencia serial se obtuvo reiniciando el JWPLC Basic y observando el Serial Monitor de VS Code. Esto se registra como mejora posible del tooling de captura, no como fallo del firmware ni de GFX.

## Estado del piloto 2 - CERRADO / ADOPTADO

Gates aprobados:

- auditoria bridge-compatible: PASS;
- ESP32 Board: PASS;
- JWPLC Basic: PASS;
- JWPLC Basic Core: PASS;
- compilacion y upload fisico: PASS;
- inicializacion real de `JWPLC_Display`: PASS;
- primitivas GFX visibles en TFT: PASS.

Conclusion:

`Adafruit_GFX_Library` puede permanecer como `precompiled=full` en Alpha5 usando el bridge GPIO generico validado. El archive historico reutilizado conserva funcionamiento cross-board y funcionamiento grafico real en el TFT del JWPLC Basic.
