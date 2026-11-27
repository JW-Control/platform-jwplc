#!/usr/bin/env bash
# Quita del package los ejemplos de arduino-esp32 que no aplican al JWPLC Basic.
# Solo borra carpetas examples/: las librerías siguen instaladas y compilan igual.
# Correr de nuevo después de cada actualización de arduino-esp32 dentro del package.
# Lista y motivos: docs/v2.1.0-alpha.18/PRE_RELEASE.md (decisión del propietario, 27/11/2026).
set -euo pipefail
LIB="$(cd "$(dirname "$0")/../../JWPLC/2.1.0/libraries" && pwd)"

# Librerías sin ningún ejemplo aplicable al JWPLC Basic (ESP32 clásico, periféricos ya soldados).
SIN_EJEMPLOS=(
  USB ESP_SR OpenThread Zigbee ESP_HostedOTA TFLiteMicro Matter   # otro chip o fuera de alcance
  SD_MMC Ethernet PPP ESP_I2S Adafruit_BusIO Wire                 # periférico soldado de otra forma
  FFat Console Insights RainMaker                                 # sin uso en el equipo
  SPI SD                                                          # se saltan el mutex del bus SPI (usar JW_SD)
)
# De la carpeta ESP32 solo quedan los ejemplos que no piden pines libres.
ESP32_QUEDAN=(ChipID MacAddress ResetReason FreeRTOS Time Timer)

for l in "${SIN_EJEMPLOS[@]}"; do
  if [ -d "$LIB/$l/examples" ]; then rm -rf "$LIB/$l/examples"; echo "sin ejemplos: $l"; fi
done
if [ -d "$LIB/ESP32/examples" ]; then
  for d in "$LIB/ESP32/examples"/*/; do
    n="$(basename "$d")"
    if [[ ! " ${ESP32_QUEDAN[*]} " == *" $n "* ]]; then rm -rf "$d"; echo "ESP32 sin: $n"; fi
  done
fi
