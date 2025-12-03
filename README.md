# casky

Casky is a simple, high-performance, embeddable key-value store written in C,
inspired by the Bitcask paper. It provides a log-structured architecture for
fast writes and reads, with optional thread-safety and crash resilience.

## Features

- **Log-structured storage**: all writes are appended to a file.
- **In-memory key directory**: fast key lookup without scanning the log.
- **Thread-safe API**: optional compile-time thread-safety using
  `-DTHREAD_SAFE`.
- **Crash recovery**: detects and handles corrupted log entries.
- **Compaction**: removes corrupted or deleted entries from the log.
- **Simple TCP server** (`caskyd`) with command-line protocol (`PUT`, `GET`,
  `DEL`, `QUIT`).
- Thread-safe API for `casky_put`, `casky_get`, `casky_delete`, and
  `casky_compact` (`-DTHREAD_SAFE`).
- Full TDD coverage: unit tests and integration tests.
- Stress test for concurrent clients against `caskyd`.
- Optional fsync on every write (`sync_on_write` flag).
- Consistent CRC checks on all records with automatic marking of corrupted logs.
- Caskyd server simplified to rely on API thread-safety instead of managing its
  own locks.
- Semantic versioning in changelog and release notes.

## Installation

```bash
git clone <repo_url>
cd casky
make
```

To build with thread-safe API:

```sh
make CFLAGS="-DTHREAD_SAFE" all
```

## Usage

### Using the library

```c
#include "casky.h"

KeyDir *db = casky_open("mydb.log");
casky_put(db, "key1", "value1");
char *v = casky_get(db, "key1");
casky_delete(db, "key1");
casky_compact(db);
casky_close(db);
free(v);

```

### Using the server (caskyd)

```sh
./build/caskyd
```

Clients can connect via TCP and issue commands:

```sh
PUT <key> <value>
GET <key>
DEL <key>
QUIT
```

Responses:

- OK on successful PUT or DEL
- VALUE <value> on GET
- NOT_FOUND if key does not exist
- ERROR <code> for errors

## Tests

Run all tests:

```sh
make test
```

- test_casky – unit tests for the library
- test_caskyd – server command tests
- test_stress_caskyd – multi-threaded stress test (requires -DTHREAD_SAFE)

## Thread-Safety

Compile-time flag -DTHREAD_SAFE enables mutex protection around all operations
(put, get, delete, compact).

If not enabled, the library behaves according to the original Bitcask paper:
single-threaded access only.

## Logging

caskyd provides optional logging with different levels (INFO, DEBUG, ERROR)
controllable via environment variables.

## License

MIT License (see LICENSE.md for details)
