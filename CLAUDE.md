# MaskedMewDB — Claude Session Context

## Your Role

You are a **teacher and guide**, not an implementer. The user writes all the code.

- Explain the *why* before the *how* for every concept
- Guide with questions and hints, not code solutions
- Review code the user shares and give constructive feedback
- Never paste a complete implementation — small illustrative snippets are okay when explaining a concept
- Assign small practice tasks before complex milestones
- Don't move to the next phase until the current one works and the user can explain it

## About the User

- **C++ level:** Intermediate — comfortable with classes, STL containers (`std::map`, `std::unordered_map`), file I/O
- **Systems/DB experience:** Beginner — explain all database and systems concepts from first principles
- **Goal:** Deep understanding of database internals, not a fast-shipping codebase

---

## Project: MaskedMewDB

A C++ LSM-tree key-value database built from scratch as a learning project.

**Commands the DB will support:**
```
SET <key> <value>    → OK
GET <key>            → VALUE <value> | NOT_FOUND
DELETE <key>         → OK
EXISTS <key>         → TRUE | FALSE
RANGE <start> <end>  → sorted key-value pairs + END
PREFIX_SCAN <prefix> → matching key-value pairs + END
STATS                → metrics summary
```

**Architecture (build order):**
```
CLI REPL → MemTable → WAL → SSTables → Compaction → Bloom Filters → TCP Server → Replication → Sharding
```

---

## Learning Roadmap

| Phase | Focus | Status |
|-------|-------|--------|
| 0 | Practice: Calculator REPL | ✅ Done |
| 1 | CMake setup + CLI REPL + in-memory KV (`std::unordered_map`) | ✅ Done |
| 2 | Ordered MemTable (`std::map`) + RANGE + PREFIX_SCAN | ✅ Done |
| 3 | Write-Ahead Log (WAL) + crash recovery | ✅ Done |
| 3a | Practice: binary file encoder/decoder | ✅ Done |
| 3b | Practice: WAL write + recover standalone | ✅ Done |
| 4 | SSTables + MemTable flush | ⬜ |
| 5 | LSM Compaction | ⬜ |
| 6 | Bloom filters + sparse index | ⬜ |
| 6a | Practice: standalone Bloom filter | ⬜ |
| 7 | Benchmark tool | ⬜ |
| 8 | TCP server + concurrency | ⬜ |
| 9 | Replication + consistent hashing | ⬜ |

---

## Current State (Phase 4 — SSTables, not started)

**Phases 1, 2, 3 complete.** All files building and tested.

### Completed files
- `calc.cpp` — calculator REPL practice task
- `CMakeLists.txt` — C++17, `src/db_engine.cpp` + `src/wal.cpp`, links zlib (`-lz`)
- `main.cpp` — full REPL, all commands wired, `db_engine Db("wal")`
- `include/db_engine.h` — `db_engine` class, `friend class wal`, `recover_set/del` private
- `src/db_engine.cpp` — `std::map` MemTable, WAL-first writes, WAL recovery on startup
- `include/wal.h` — `wal` class, forward declares `db_engine`
- `src/wal.cpp` — `write()`, `recover()`, `calc_checksum()`, `wal_data` struct
- `practice/encoder.cpp` — binary encoder/decoder practice (complete)
- `practice/wal.cpp` — standalone WAL write + recover practice (complete)

### Correct response format
```
SET name sarthak   → OK
GET name           → VALUE sarthak
GET missing        → NOT_FOUND
DELETE name        → OK | NOT_FOUND
EXISTS name        → TRUE | FALSE
KEYS               → one key per line + END
RANGE a z          → key value (one per line) + END
PREFIX_SCAN user:  → key value (one per line) + END
```

### WAL record format
```
[magic_number]   uint32_t  4 bytes   (0xDEADBEEF)
[version_number] uint8_t   1 byte
[index]          uint64_t  8 bytes   (sequence number, auto-incremented by wal)
[operation]      uint8_t   1 byte    (0=SET, 1=DELETE)
[len_of_key]     uint32_t  4 bytes
[val_of_key]     char[]    variable
[len_of_data]    uint32_t  4 bytes
[val_of_data]    char[]    variable
[checksum]       uint32_t  4 bytes   (CRC32 over version→val_of_data)
```

### Key design decisions (WAL)
- `wal` owns the file — opens in constructor (`binary|in|app`), closes in destructor
- File created before open via temporary `ofstream` if it doesn't exist
- `db_engine` owns `wal*` via pointer (forward declaration requires pointer for member)
- `recover()` uses `db.recover_set()` / `db.recover_del()` — bypasses WAL during replay
- `friend class wal` grants access to private `recover_set/del` methods
- `wal_log_file.clear()` after recovery resets EOF flag before writes begin
- WAL write must succeed before MemTable update (durability — the D in ACID)
- `index` updated to highest seen sequence number during recovery

### Concepts understood
- CRC32 checksums vs cryptographic hashes ✅
- Why WAL is append-only (sequential writes + crash safety) ✅
- Magic numbers and version bytes ✅
- Fixed-width unsigned types (`uint8_t`, `uint32_t`, `uint64_t`) ✅
- Forward declarations and when to use pointers vs references ✅
- `friend class` — grants private access to tightly coupled classes ✅
- `file.clear()` to reset stream error/EOF flags ✅
- Member variables vs function parameters (sizing implications) ✅
- Why WAL grows forever and needs truncation after MemTable flush ✅ (discussed, not yet implemented)

### Open question for next session
The WAL grows forever — every SET/DELETE appends a record. After a MemTable flush to SSTable, old WAL records are no longer needed. How should this be handled? (Answer: WAL truncation / checkpointing after flush — to be implemented alongside SSTables in Phase 4.)

### Naming convention
- snake_case for all classes and variables (e.g. `db_engine`, `wal`, `wal_data`)
- Project headers use `#include "file.h"`, system headers use `#include <file>`

---

## Key Patterns Already Established

**REPL pattern** (from `calc.cpp`):
```
loop:
  print prompt
  getline(cin, line)
  if exit/quit → break
  if empty → continue
  parse line into tokens
  dispatch on command
  print result
```

**Error format:** `ERROR: <message>` (consistent across all error cases)

**Prompt:** `mew> `

---

## File Structure

```
mewDb/
├── CLAUDE.md              ← this file
├── CMakeLists.txt         ← complete and building cleanly
├── main.cpp               ← full REPL, all commands wired
├── calc.cpp               ← practice REPL (complete, not part of main build)
├── include/
│   ├── db_engine.h        ← db_engine class (friend class wal, recover_set/del)
│   └── wal.h              ← wal class declaration
├── src/
│   ├── db_engine.cpp      ← db_engine implementation (std::map + WAL integration)
│   └── wal.cpp            ← WAL implementation (write, recover, checksum)
├── practice/
│   ├── encoder.cpp        ← binary encoder/decoder practice (complete)
│   └── wal.cpp            ← standalone WAL write + recover practice (complete)
└── build/                 ← generated by cmake (gitignored)
```
