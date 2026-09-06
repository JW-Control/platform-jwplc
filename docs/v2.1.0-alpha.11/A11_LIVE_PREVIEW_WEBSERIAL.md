# Alpha11 — Live Preview físico por Web Serial

## Objetivo

Permitir que el usuario edite la HMI en `JWPLC HMI Designer` y vea el resultado en la TFT del JWPLC Basic sin recompilar/subir el sketch en cada cambio.

```text
Designer localhost
      |
      | Web Serial / USB
      v
JWPLC HMI Live Bridge
      |
      v
ST7789 física
```

## Decisión de arquitectura

El modo LIVE es una herramienta de diseño y diagnóstico. **No reemplaza** el runtime declarativo ni el codegen público.

```text
PRODUCTION_CODEGEN=JWPLC_UI_PUBLIC_API
LIVE_TRANSPORT=FRAMEBUFFER_RGB565_RLE
SECOND_HMI_RUNTIME=NO
LIVE_PERSISTENCE=NO
```

El C++ final continúa usando:

```cpp
JWPLC_UITextField(...)
JWPLC_UIValueField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
JWPLC_Display.setFields(...)
```

El Designer no genera `jwplcUIUpdate()`.

## Por qué transportar framebuffer y no structs

Durante Alpha11 todavía se están cerrando `VALUE`, `BOOL`, `BAR` y páginas. Un protocolo basado en structs obligaría a versionar el transporte cada vez que cambie el modelo de fields.

El framebuffer RGB565:

- reproduce exactamente la `Vista previa 1:1`;
- cubre TEXT/VALUE/BOOL/BAR futuros sin cambiar el protocolo;
- cubre también Pixel y GFX RAW;
- mantiene el protocolo LIVE desacoplado del codegen final;
- permite comparar navegador vs TFT física pixel a pixel.

## Protocolo A11-LIVE/1

Puerto:

```text
baud=921600
transport=Web Serial
frame=320x170 RGB565
compression=RLE
```

Header binario:

```text
4 bytes  magic      = "JWH1"
4 bytes  sequence   = uint32 little-endian
4 bytes  runCount   = uint32 little-endian
2 bytes  width      = 320
2 bytes  height     = 170
```

Cada run:

```text
2 bytes  pixelCount = uint16 little-endian
2 bytes  color      = RGB565 uint16 little-endian
```

El bridge responde por Serial:

```text
JWHMI_LIVE_READY 1
JWHMI_LIVE_FRAME <sequence>
```

## Bridge físico

Sketch:

```text
tools/jwplc-hmi-designer/gates/
A11_LivePreview_WebSerial/
JWPLC_HMI_Live_Bridge/
JWPLC_HMI_Live_Bridge.ino
```

El bridge:

1. deja `JWPLC_Display` en USER;
2. usa refresco `USER_REFRESH_ON_DEMAND`;
3. recibe RLE por USB;
4. reconstruye un framebuffer RGB565 de 320x170;
5. solicita refresh;
6. pinta el framebuffer desde `jwplcUIUpdate()`.

La llamada directa a `JWPLC_Display.tft()` queda **encapsulada únicamente en este sketch de desarrollo** para copiar el framebuffer físico. El codegen del usuario no genera `tft.*`.

## Navegador

Requiere Web Serial, por lo que el gate se valida en Chrome/Edge de escritorio sobre `localhost`/HTTPS.

La UI agrega:

```text
Conectar JWPLC
LIVE desconectado
```

Flujo esperado:

```text
1. subir una vez JWPLC_HMI_Live_Bridge.ino
2. abrir Designer en localhost
3. pulsar Conectar JWPLC
4. seleccionar el COM del JWPLC Basic
5. esperar "JWPLC Basic · LIVE listo"
6. editar canvas/inspector
7. observar cambio en TFT física sin recompilar
```

El navegador sólo transmite cuando detecta cambios en el framebuffer.

## Naming por defecto

Se mantiene separación semántica entre nombre humano e identificador C++:

```text
TEXT 1   -> FIELD_TEXT_1   -> texto1
VALUE 2  -> FIELD_VALUE_2  -> valor2
BOOL 3   -> FIELD_BOOL_3   -> estado3   (pendiente A11-3C)
BAR 4    -> FIELD_BAR_4    -> valor4    (pendiente A11-3D)
```

`Nombre del objeto` no es `Etiqueta visible`.

## Gate pendiente

```text
A11_LIVE_WEBSERIAL_IMPLEMENTED=YES
A11_LIVE_WEBSERIAL_PHYSICAL_GATE=PENDING
A11_LIVE_BROWSER_TO_TFT_PARITY=PENDING
```

Antes de cerrar Alpha11 se debe comprobar:

- handshake READY;
- primera transferencia completa;
- cambio de texto/color/posición/tamaño en vivo;
- Pixel/GFX RAW en vivo;
- desconexión/reconexión;
- ausencia de resets inesperados;
- paridad visual con `Vista previa 1:1`.
