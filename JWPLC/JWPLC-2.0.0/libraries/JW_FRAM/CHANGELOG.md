# Changelog

## [1.0.2] - 2026-04-24

### Added
- Added optional SPI bus lock/unlock callbacks for shared SPI bus environments.
- Added `setBusLockCallbacks()` and `clearBusLockCallbacks()`.
- Added internal SPI helper methods to route FRAM SPI transfers through optional bus locks.

### Changed
- Internal SPI transfers now use wrapper helpers instead of calling `Adafruit_SPIDevice` directly.
- Preserved backward compatibility when no bus lock callbacks are configured.

### Notes
- This version improves compatibility with JWPLC hardware families using shared SPI peripherals such as TFT, FRAM, microSD and Ethernet.


## [1.0.1] - 2026-04-03
### Añadido
- Estructura completa de librería compatible con el ecosistema Arduino.
- Archivos `CHANGELOG.md`, `keywords.txt` y `LICENCE`.
- README en español.
- API de bajo nivel para FRAM SPI.
- API de alto nivel estilo EEPROM con `get`, `put` y `update`.
- Soporte para `String`, C strings, structs trivially copyable y bloques con header.
- Soporte de depuración mediante `Stream`.
- Soporte para tamaño forzado de FRAM en `begin()`.

## [1.0.0] - 2026-04-03
### Inicial
- Primera propuesta funcional de JW_FRAM.
