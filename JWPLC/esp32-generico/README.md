# Placas genéricas (package «ESP Board», packager `jwesp`)

`boards.txt` de ESP32 Board (`jwesp:esp32:esp32`) y ESP32 Family Device (oculta). El resto de la plataforma no se
versiona aquí: `JWPLC/tools/jwplc_esp32_generico.py` la arma al publicar desde el core del package `JWPLC/<versión>`
sin las librerías JW/JWPLC y con los ejemplos originales de arduino-esp32.

Motivo y pruebas: `docs/v2.1.0-alpha.19/A19_LIBRERIAS_POR_PLACA_20261128.md`.

## Probar en Arduino IDE sin publicar

El JWPLC Basic se prueba con `jwplc_local`, una junction a `JWPLC/2.1.0` que se actualiza sola. ESP Board no existe
como carpeta en el repo, así que se arma e instala con un comando (Arduino IDE cerrado):

```powershell
powershell -ExecutionPolicy Bypass -File tools\alpha19\actualizar_esp_board_local.ps1
```

Escribe en `%LOCALAPPDATA%\Arduino15\packages\jwesp_local\hardware\esp32\2.1.0`. Los ejemplos de arduino-esp32 se
descargan la primera vez y quedan en caché.
