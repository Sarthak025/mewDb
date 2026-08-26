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
| 3 | Write-Ahead Log (WAL) + crash recovery | 🔄 In progress |
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

## Current State (Phase 3 — WAL, in progress)

**Phases 1 & 2 complete.** All files building and tested:
- `calc.cpp` — calculator REPL practice task
- `CMakeLists.txt` — C++17, includes `src/db_engine.cpp`, exports compile commands
- `include/db_engine.h` — `db_engine` class with `set`, `get`, `del`, `exists`, `keys`, `range`, `prefix_scan`
- `src/db_engine.cpp` — implemented with `std::map<string, string>`
- `main.cpp` — full REPL wired to `db_engine`, all responses match spec

**Correct response format (established):**
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

**Phase 3a & 3b complete — WAL practice (`practice/wal.cpp`)**

Concepts understood:
- Checksums vs cryptographic hashes — CRC32 for speed, not security ✅
- Why WAL is append-only — sequential write performance + crash safety ✅
- Magic numbers and version bytes ✅
- Fixed-width unsigned types (`uint8_t`, `uint32_t`, `uint64_t`) ✅
- Passing strings by `const&` to avoid copies ✅
- Reading into `std::string` via `.data()` ✅
- `file.clear()` needed before reopening a stream ✅

WAL record format (user-designed and implemented):
```
[magic_number]   uint32_t  4 bytes   (0xDEADBEEF)
[version_number] uint8_t   1 byte
[index]          uint64_t  8 bytes   (sequence number)
[operation]      uint8_t   1 byte    (0=SET, 1=DELETE)
[len_of_key]     uint32_t  4 bytes
[val_of_key]     char[]    variable
[len_of_data]    uint32_t  4 bytes
[val_of_data]    char[]    variable
[checksum]       uint32_t  4 bytes   (CRC32 over version→val_of_data)
```

Key code patterns in `practice/wal.cpp`:
- `wal_data` struct holds all record fields including strings
- `calc_checksum(const wal_data&)` — reusable by both write and recover
- `write_record_v1` — builds struct, computes CRC, writes fields in order
- `recover` — seeks to start, reads field-by-field, verifies CRC, prints ops

**Next task: wire WAL into the real DB**
1. Create `include/wal.h` and `src/wal.cpp` with a `WAL` class
2. `db_engine` gets a `WAL` member
3. Every `set()` and `del()` calls `wal.write(...)` before touching `std::map`
4. On startup, `db_engine` calls `wal.recover(...)` to rebuild MemTable state

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
│   └── db_engine.h        ← db_engine class declaration (complete)
├── src/
│   └── db_engine.cpp      ← db_engine implementation (complete, std::map)
├── practice/
│   ├── encoder.cpp        ← binary encoder/decoder practice (complete)
│   └── wal.cpp            ← standalone WAL write + recover practice (complete)
└── build/                 ← generated by cmake (gitignored)
```
