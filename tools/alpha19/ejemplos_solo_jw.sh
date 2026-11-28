#!/usr/bin/env bash
# Deja en el package del JWPLC Basic solo los ejemplos de las librerías JW_ y JWPLC_.
# Arduino IDE arma «Ejemplos para JWPLC Basic» con todas las librerías de la plataforma (no filtra por placa).
# Las librerías del core de arduino-esp32 siguen instaladas y compilan igual (#include <WiFi.h> funciona);
# solo se borran sus carpetas examples/. Los ejemplos del core se ven con ESP32 Board (packager jwesp), cuya
# plataforma arma JWPLC/tools/jwplc_esp32_generico.py con los ejemplos originales de arduino-esp32.
# Reemplaza a tools/alpha18/podar_ejemplos_esp32.sh. Correr de nuevo después de reimportar arduino-esp32.
# Decisión del propietario: docs/v2.1.0-alpha.19/A19_LIBRERIAS_POR_PLACA_20261128.md
set -euo pipefail
LIB="$(cd "$(dirname "$0")/../../JWPLC/2.1.0/libraries" && pwd)"

# Librerías del core cuyos ejemplos sí se ven con el JWPLC Basic (vacío = ninguno).
EXCEPCIONES=()

for d in "$LIB"/*/; do
  n="$(basename "$d")"
  case "$n" in JW_*|JWPLC_*) continue ;; esac
  if [[ " ${EXCEPCIONES[*]-} " == *" $n "* ]]; then continue; fi
  if [ -d "$d/examples" ]; then rm -rf "$d/examples"; echo "sin ejemplos: $n"; fi
done
