# NovaDB

NovaDB is an educational C++17 key-value storage engine inspired by the broad LSM-tree architecture used by Google LevelDB. It is an independent implementation and does not copy LevelDB source code.

## Implemented in this version

- Ordered in-memory index (`std::map`) with last-write-wins sequence numbers.
- Append-only write-ahead log (WAL) with record framing and FNV-1a checksums.
- Recovery from the latest complete snapshot plus valid WAL records.
- Immutable sorted snapshot files (`.sst`) written through a temporary file and rename.
- Put, get, delete, batch API, ordered scan, explicit flush, and automatic flush by approximate memory threshold.
- CMake build, demo, and basic regression tests.

## Important limitations

This is a learning-grade first milestone, not a production database. It currently uses a single snapshot SSTable rather than LevelDB's multi-level table/version system. The in-memory index is a `std::map`, not a concurrent skip list. There is no background compaction, Bloom filter, block cache, iterator object, snapshot isolation, compression, multi-process locking, or Python extension yet. `WriteBatch` currently appends its operations as individual WAL records, so it is not crash-atomic as a group. WAL writes use standard C++ stream flushing and do not issue platform-specific `fsync`/`FlushFileBuffers`; power-loss durability is therefore not guaranteed. One process should own a database directory at a time.

## Requirements

- CMake 3.16 or newer
- C++17 compiler (GCC 9+, Clang 10+, or recent MSVC)

## Build on Windows (PowerShell)

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\build\Release\novadb_demo.exe
.\build\Release\novadb_quickstart.exe
```

For a single-configuration generator such as MinGW Makefiles:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
.\build\novadb_demo.exe
.\build\novadb_quickstart.exe
```

## Build on Linux/macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/novadb_demo
./build/novadb_quickstart
```

## API example

```cpp
#include "novadb/db.hpp"
#include <iostream>

int main() {
    auto db = novadb::DB::open("./my_database");

    db.put("name", "Ahmad");
    db.put("field", "Computer Science");

    auto result = db.get("name");
    if (result) {
        std::cout << *result << std::endl;
    }

    db.erase("field");
    db.flush();
    return 0;
}
```

## On-disk format (v0.1)

- `active.wal`: records with `NWL1` magic, payload length, operation/sequence/key/value payload, and FNV-1a checksum.
- `table-N.sst`: `NST1` magic, snapshot sequence, entry count, then sorted key/sequence/live-value entries.
- On flush, NovaDB writes `table-(N+1).sst.tmp`, publishes it by rename, removes older snapshots, then truncates the WAL. Recovery selects the highest numbered snapshot and replays valid WAL records with a sequence newer than the snapshot.

This format is experimental and may change between versions.

## Roadmap

1. Atomic batch WAL records and platform-specific durable sync.
2. Multiple SSTables, manifest/version metadata, and crash-safe file lifecycle.
3. Bloom filters and block-based table format.
4. LSM levels and leveled compaction.
5. Snapshot reads, iterators, and concurrent readers/writers.
6. Python bindings and benchmarks.

## Research basis

LevelDB's implementation documentation describes log files, an in-memory memtable, immutable sorted tables, a manifest, and compaction. Its implementation separates DB orchestration, memtable, log reader/writer, table building, and version management. NovaDB uses those concepts as architectural inspiration while keeping its initial code and file format independent.
