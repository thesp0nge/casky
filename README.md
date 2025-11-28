# casky

## Description

Casky is a lightweight, crash-safe key-value store written in C, designed for
fast storage and retrieval of data with a minimal footprint. Built using
Test-Driven Development (TDD), Casky ensures reliability while keeping the
codebase clean and maintainable. It is inspired by Bitcask and aims to provide a
simple, embeddable storage engine that can be integrated into microservices, IoT
devices, and other C-based applications.

### Objectives:

- Implement a minimal key-value store with append-only file storage.
- Support crash-safe persistence and recovery.
- Expose a simple public API: store(key, value), load(key), delete(key).
- Follow TDD methodology for robust and testable code.
- Provide a foundation for future extensions, such as in-memory caching,
  compaction, and eventual integration with vector-based databases like PixelDB.

### Why This Project is Interesting:

Casky combines low-level C programming with modern database concepts, making it
an ideal playground to explore storage engines, crash safety, and performance
optimization. It’s small enough to complete during Hackweek, yet it provides a
solid base for future experiments and more complex projects.

## Goals

- Working prototype with append-only storage and memtable.
- TDD test suite covering core functionality and recovery.
- Demonstration of basic operations: insert, load, delete.
- Optional bonus: LRU caching, file compaction, performance benchmarks.

### Future Directions:

After Hackweek, Casky can evolve into a backend engine for projects like
PixelDB, supporting vector storage and approximate nearest neighbor search,
combining low-level performance with cutting-edge AI retrieval applications.

## Resources

The Bitcask paper:
[https://riak.com/assets/bitcask-intro.pdf](https://riak.com/assets/bitcask-intro.pdf)
The Casky repository:
[https://github.com/thesp0nge/casky](https://github.com/thesp0nge/casky)
