# Changelog

All notable changes to this project will be documented in this file.

## [1.0.4] - 2026-05-20

### Fixed
- Fixed compilation error in `clearEventQueue() const` and `clearPendingInput() const` by marking `_evN` as `mutable`.
- Fixed ESP32 task stack handling: `startTask(..., stackBytes, ...)` now passes stack size as bytes, matching Arduino-ESP32 / ESP-IDF behavior.

### Documentation
- Updated `library.properties` version to `1.0.4`.
- Updated README repeat defaults to match the actual implementation.
- Clarified column input wiring: the library configures columns as `INPUT` and does not enable `INPUT_PULLUP` internally.
- Added a short README section for optional ESP32 task usage.

## [1.0.3] - 2026-04-03

### Added
- Added pending-input cleanup helpers:
  - `clearPendingPresses()`
  - `clearPendingReleases()`
  - `clearPendingRepeats()`
  - `clearEventQueue()`
  - `clearPendingInput()`

### Improved
- Improved library ergonomics for multi-screen / HMI-style applications.
- README updated with guidance for safe screen transitions.
- `keywords.txt` aligned with actual public API names.

## [1.0.2] - 2026-02-19

### Changed
- Metadata / packaging update.

## [1.0.0] - 2026-02-19

### Added
- Initial release.
- Matrix scanning (rows/columns).
- Debounce handling.
- Press / Release / Repeat events.
- Key repeat acceleration.
- Event queue support.