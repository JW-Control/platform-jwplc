# PORTING MAP JWPLC 2.0.0

## Referencias
- Base oficial vieja: ESP32 3.0.7
- Base oficial nueva: ESP32 3.3.8
- Base JWPLC actual: 1.0.5

## Archivos a revisar primero
- [ ] boards.txt
- [ ] platform.txt
- [ ] programmers.txt
- [ ] variants/
- [ ] cores/esp32/
- [ ] libraries/
- [ ] tools/
- [ ] package_jwplc_index.json

## Objetivo
Reconstruir JWPLC 2.0.0 limpio sobre ESP32 3.3.8,
portando solo los cambios realmente necesarios desde JWPLC 1.0.5.