# Changelog

## 1.0.5

### Added

- Agrega modo de escaneo directo mediante `beginDirect(...)`.
- Permite usar botones independientes 1×N sin declarar filas, columnas ni `BtnMapItem`.
- Agrega backend interno de escaneo directo con debounce por botón.
- Conserva los mismos eventos para modo directo:
  - `EV_PRESS`
  - `EV_RELEASE`
  - `EV_REPEAT`
- Mantiene compatibilidad de `pressed(id)`, `released(id)`, `isDown(id)`, `eventCount()`, `getEvent()`, `clearPendingInput()`, `applyAxis()` y `startTask()` en modo directo.
- Documenta el modo de botones directos en README.

### Changed

- `update()` ahora delega internamente según el modo configurado:
  - matriz R×C;
  - botones directos 1×N.
- README actualizado para describir ambos modos de uso.

### Compatibility

- La API existente de matriz `begin(...)` se mantiene sin cambios.
- El modo matriz sigue usando `rowPins`, `colPins` y `BtnMapItem`.
- El modo directo no configura ningún pin como `OUTPUT`; solo aplica `pinMode(buttonPins[i], inputMode)`.

## 1.0.4

### Added

- Eventos latcheados para `pressed()` y `released()`.
- Cola interna de repeats pendientes.
- Helpers de limpieza:
  - `clearPendingPresses()`
  - `clearPendingReleases()`
  - `clearPendingRepeats()`
  - `clearEventQueue()`
  - `clearPendingInput()`
- Soporte opcional de task en ESP32.
- Helper `applyAxis()` para navegación de valores en interfaces HMI/PLC.
