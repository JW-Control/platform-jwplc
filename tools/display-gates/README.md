# Gates de banco de JWPLC_Display

Sketches de prueba que **no** son ejemplos para usuarios: validan en placa reglas del Display y dejan el
resultado por Serial (115200) como `[PASS]` / `[FAIL]`. Viven fuera de `JWPLC/2.1.0/libraries/` para no
aparecer en *Archivo → Ejemplos* del Arduino IDE.

| Gate | Qué valida |
|---|---|
| `Display_HMI_Botonera_Gate` | Por defecto el Display no despierta USER con una tecla (180 s en IDLE sin auto-wake) y no consume los latches `pressed()`/`released()` de la aplicación (CLAUDE.md §3.5). Antes era el ejemplo `Display_Alpha8_HMI_Gate` (Alpha8). |

Compilar y subir con el package de la rama (`jwplc_local`):

```powershell
arduino-cli compile --upload -b jwplc_local:esp32:jwplcbasic -p COM6 tools\display-gates\Display_HMI_Botonera_Gate
```

En la nube o WSL: `tools/alpha15/regenerate_libJWPLC_TFT.sh compile tools/display-gates/Display_HMI_Botonera_Gate`.
