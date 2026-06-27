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
| 3 | Write-Ahead Log (WAL) + crash recovery | ⬜ |
| 3a | Practice: binary file encoder/decoder | 🔄 In progress |
| 4 | SSTables + MemTable flush | ⬜ |
| 5 | LSM Compaction | ⬜ |
| 6 | Bloom filters + sparse index | ⬜ |
| 6a | Practice: standalone Bloom filter | ⬜ |
| 7 | Benchmark tool | ⬜ |
| 8 | TCP server + concurrency | ⬜ |
| 9 | Replication + consistent hashing | ⬜ |

---

## Current State (Phase 3a — in progress)

**Phase 1 complete.** All files building and tested:
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

**Phase 3a — Practice: binary encoder/decoder (`practice/encoder.cpp`)**
- Writes strings to a binary file as `[uint32_t length][string bytes]`
- Reads them back correctly
- Uses `reinterpret_cast<char*>`, `std::ios::binary`, `file.read/write`
- `uint32_t` used for length field (platform-independent, unsigned) ✅
- RAII understood — destructor closes file, explicit `close()` after `return` is dead code ✅
- Status: **complete**, pending one concept discussion before moving to WAL

**The open question before WAL:**
The user was asked: *"What is a checksum conceptually, and where in the WAL record format would it go?"* — answer this first, then move to WAL.

**WAL record format target (from PRD):**
```
[uint32_t magic][uint8_t version][uint32_t record_length][uint64_t sequence_no]
[uint8_t op_type][uint32_t key_length][uint32_t value_length][key_bytes][value_bytes][uint32_t checksum]
```

**Why the practice task before WAL:**
WAL requires writing binary data to files — fixed-width integers, length-prefixed byte sequences. This is very different from `cout`. The practice task builds comfort with `fstream` binary mode, `reinterpret_cast`, and reading/writing raw bytes before adding the complexity of WAL semantics.

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
│   └── encoder.cpp        ← binary encoder/decoder practice (complete)
└── build/                 ← generated by cmake (gitignored)
```
