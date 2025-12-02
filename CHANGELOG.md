# Changelog for Casky

All notable changes to this project will be documented in this file.

## [0.20.0] - 2025-12-02

### Added

- Append-only log support: every PUT and DELETE operation is now persisted to a
  log file in Bitcask format: `[CRC][Timestamp][KeyLen][ValueLen][Key][Value].`
- casky_write_data_to_file() function for atomic log writes with optional fsync.
- sync_on_write flag in KeyDir to control whether each write is flushed to disk.
- casky_crc32() function to calculate CRC32 checksums of log records.
- casky_put_in_memory() and casky_delete_from_memory() helper functions to
  separate memory and disk logic.
- casky_open() now reconstructs the KeyDir from the log file, skipping corrupted
  records and setting corrupted_dir flag.
- casky_compact() API: creates a compacted log file removing corrupted entries
  and outdated keys.
- corrupted_dir field in KeyDir to signal the presence of corrupted records.
- Tests for log integrity, including crash-resilient reads and writes.
- casky_logdump tool to inspect the contents of a log file, verifying CRC.

### Changed

- casky_put() and casky_delete() now call in-memory helpers and write to log
  atomically.
- casky_open() sets casky_errno to CASKY_ERR_CORRUPT when corrupted records are
  detected.
- Updated TDD tests to reflect append-only log behavior and crash recovery
  semantics.
- Error handling improved for memory allocation and I/O failures.
- Logging of DELETE operations in the append-only log.

### Fixed

- Memory leaks in log reading routines.
- Timestamp handling in log writes, ensuring correct reconstruction during
  casky_open().
- CRC verification now applied correctly to both keys and values.
- test_put_writes_log() and test_log_integrity() corrected to match the new
  append-only logic

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
