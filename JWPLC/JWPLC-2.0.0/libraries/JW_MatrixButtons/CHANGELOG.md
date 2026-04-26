# Changelog

All notable changes to this project will be documented in this file.

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