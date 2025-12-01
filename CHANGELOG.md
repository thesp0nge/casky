# Changelog for Casky

All notable changes to this project will be documented in this file.

## [0.10.0] - 2025-12-01

### Added

- Core in-memory KeyDir and EntryNode structures
- API functions: `casky_open`, `casky_close`, `casky_put`, `casky_get`,
  `casky_delete`
- Hash function: `casky_djb2_hash_xor`
- Error handling via `casky_errno`
- Unit tests for all APIs using standard asserts
- Test cleanup of temporary files

### Changed

- None (first MVP)

### Fixed

- None (first MVP)
