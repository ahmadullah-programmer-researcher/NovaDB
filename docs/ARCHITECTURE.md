# NovaDB architecture notes

## Write path

A `put` or `delete` receives a monotonically increasing sequence number. NovaDB appends a framed record to `active.wal`, flushes the C++ stream, then updates the ordered in-memory map. The sequence number lets replay ignore operations already represented in the latest snapshot.

## Read path

The current milestone keeps the latest state in one ordered map. A lookup is O(log N). A scan traverses the map in key order and omits tombstones.

## Flush and recovery

Flush serializes the complete current state to a new immutable snapshot. It first writes a temporary file, closes it, and renames it to its final name. Only after publishing the new snapshot does it remove older snapshots and truncate the WAL. Recovery chooses the highest generation snapshot, then replays checksum-valid WAL records whose sequence is newer than the snapshot sequence. A torn or malformed WAL tail is ignored.

## Relationship to LevelDB

LevelDB's public implementation uses a log-backed memtable and sorted table files, with version/manifest metadata and background compaction. Its source has distinct modules for DB coordination, memtable, log reader/writer, table building, and version management. NovaDB deliberately starts smaller: one map and one whole-state snapshot. This makes the persistence lifecycle easier to study before implementing a true multi-level LSM tree.

## Complexity

- `get`: O(log N)
- `put`/`delete`: O(log N) map update plus append cost
- `scan`: O(N)
- `flush`: O(N) serialization and disk write
- Space: O(N) in-memory latest-state map, plus WAL until flush

These are educational estimates, not benchmark claims.
