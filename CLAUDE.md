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
| 4 | SSTables + MemTable flush | ✅ Done (includes tombstone/delete fix) |
| 5 | LSM Compaction | ⬜ |
| 6 | Bloom filters + sparse index | ⬜ |
| 6a | Practice: standalone Bloom filter | ⬜ |
| 7 | Benchmark tool | ⬜ |
| 8 | TCP server + concurrency | ⬜ |
| 9 | Replication + consistent hashing | ⬜ |

---

## Current State (Phase 5 — LSM Compaction, not started)

**Phases 1–4 complete**, including the tombstone/delete fix (see below). All files building and tested. Full read/write cycle (WAL → MemTable → SSTable flush → SSTable read-back, tombstone-aware) works end-to-end and has been verified by actually building and running the project, not just reading the code. One deliberate loose end remains — see "Known gap, sequenced" below (`EXISTS`/`RANGE`/`PREFIX_SCAN` not yet searching SSTables) — before moving fully into Phase 5.

### Completed files
- `calc.cpp` — calculator REPL practice task
- `CMakeLists.txt` — C++17, sources: `main.cpp`, `src/db_engine.cpp`, `src/wal.cpp`, `src/ss_table.cpp`, `src/manifest.cpp`; links zlib (`-lz`)
- `main.cpp` — full REPL, all commands wired, `db_engine Db;` (no-arg constructor — filenames now come from `constants.h`)
- `include/constants.h` — every shared format constant lives here: `WAL_VERSION`/`WAL_MAGIC_NUMBER`, `SS_TABLE_VERSION`/`SS_TABLE_MAGIC_NUMBER`, `MANIFEST_VERSION`/`MANIFEST_MAGIC_CONST`, `enum class operation : uint8_t { set, del }` (replaces the old loose `SET`/`DELETE` constants, shared by WAL/`db_engine`/`ss_table`), file name constants (`WAL_FILE_NAME`, `SS_TABLE_NAME`, `MANIFEST_FILE_NAME`, `MANIFEST_TEMP_FILE_NAME` — all under `data/`), `MEM_TABLE_SIZE_LIMIT`, and the `open_mode` enum (`read`/`write`) used by `ss_table`
- `include/db_engine.h` / `src/db_engine.cpp` — `db_engine` class; MemTable is `curr_mem_table` (renamed from `storage`), value type `std::optional<std::string>` (`nullopt` = tombstone); owns `wal*` and `manifest*` (no longer owns its own SSTable-index counter — always asks the manifest); `flush()` returns `bool`; `exists()` renamed `exists_in_curr_mem_table()` to be explicit it doesn't search SSTables yet
- `include/wal.h` / `src/wal.cpp` — adds `truncate()` (close → reopen via `std::ofstream` with `trunc` → reopen `in|app`); `index` never resets, even across truncation (LSN-style, for future replication)
- `include/ss_table.h` / `src/ss_table.cpp` — `write_to_ss_table()` (flush path, tombstone-aware) and `get_value_from_ss_table()` (read path, returns the 3-state `lookup_result`; `open_mode::read`/`write` selects which)
- `include/manifest.h` / `src/manifest.cpp` — durable registry of valid SSTable indices (see below)
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

**Known gap, sequenced — steps 1–2 done, step 3 is next:** `EXISTS`, `RANGE`, and `PREFIX_SCAN` still only look at the live MemTable (`exists_in_curr_mem_table()`'s name is deliberately explicit about this, with a `// TODO: need to add ss_table search later`; `range()`/`prefix_scan()` carry the same TODO). They'll miss flushed data — e.g. `EXISTS` on a key that's only in an SSTable currently returns `FALSE`. (1) tombstone/delete fix at the MemTable level — **done**. (2) SSTable format extended to support tombstones — **done**. (3) extend `EXISTS`/`RANGE`/`PREFIX_SCAN` to search SSTables the same way `get()` does, tombstone-aware (a key deleted in a newer SSTable must shadow a real value in an older one) — **not started, this is the next task**.

**Tombstone/delete fix — completed 2026-09-06, closes out the reopened part of Phase 4.** Root problem: `DELETE` only ever removed a key from `curr_mem_table`; a key `SET` then flushed to an SSTable, then `DELETE`d, could resurface via `GET` once the MemTable no longer remembered the deletion. Found while scoping the SSTable read path, not while it was being built — a miss worth remembering when reviewing future changes to shared read/write paths. What landed:
1. `SET`/`DELETE` unified into a real `enum class operation : uint8_t { set, del }` in `constants.h` (shared by WAL, `db_engine`, and `ss_table`) — replacing the old loose `uint8_t SET/DELETE` constants. `del` not `delete`, since `delete` is a reserved C++ keyword. `sizeof(operation)` matters here — an enum with no explicit underlying type defaults to `int` (4 bytes), which silently would have grown every WAL record; pinning `: uint8_t` keeps the original 1-byte footprint.
2. `curr_mem_table` is `std::map<std::string, std::optional<std::string>>` — `nullopt` *is* a tombstone, no separate operation flag needed in memory (unlike the on-disk WAL/SSTable formats, which still need an explicit `operation` byte since a flat binary layout can't represent "absence" the way `std::optional` can — a zero-length value on disk is ambiguous with a legitimate `SET key ""`, but `nullopt` in memory never is). `del()`/`recover_del()` unconditionally insert a tombstone (`curr_mem_table[key] = std::nullopt`) rather than gating it on whether the key is currently present — a key deleted after already being flushed still needs a tombstone written, since it might only exist in an older SSTable. `del()` always returns `true`/OK for now (matching the "MemTable-only" limitation already accepted for `EXISTS` above — accurately answering "did this key ever exist" needs SSTable search, which is deferred to the same step 3). `get()` checks *presence* in the MemTable (`find() != end()`), not `.has_value()` — a tombstone's mere presence must short-circuit and return `nullopt` without falling through to SSTable search; checking has-value instead was tried and caused a real regression (tombstone stopped shadowing older SSTable data) before being caught. `exists_in_curr_mem_table()`/`keys()`/`range()`/`prefix_scan()` all check `.has_value()` to exclude tombstones from their output.
3. SSTable entries gained a per-entry `operation` byte (`set`/`del`), folded into the per-entry checksum alongside `key_len`/`key`/`val_len`/`val`. A `del` entry still writes `val_len=0` (empty val), matching the WAL's existing convention rather than making entries variable-shaped.
4. `get_value_from_ss_table()` returns a 3-state `lookup_result` (declared in `ss_table.h`, not `constants.h`, since it's specific to this one method's interface): `enum class lookup_status { not_found, tombstone, found }` + `std::optional<std::string> value` (meaningful only when `found`). This was necessary because `std::optional<std::string>` alone can only represent two states, and conflating "key not in this file" with "key tombstoned in this file" would silently break cross-file shadowing the same way the MemTable regression in point 2 did. `db_engine::get()`'s SSTable loop: `not_found` → check the next older file; `tombstone` or `found` → return `result.value` immediately (correct for both, since `value` is already `nullopt` for a tombstone).

Verified end-to-end (real build, real run, not just reading code): delete of an already-flushed key now correctly returns `OK` and a later `GET` returns `NOT_FOUND`; a MemTable tombstone correctly shadows an older SSTable value; a crash + restart (WAL replay via `recover_del()`) preserves the tombstone with no flush in between; `KEYS`/`RANGE`/`PREFIX_SCAN` correctly exclude tombstoned keys from live MemTable output.

Bugs hit and fixed along the way, worth remembering for future review in this project: an enum-to-pointer `reinterpret_cast<char*>(op)` (missing `&op`) compiled cleanly — `reinterpret_cast` legally allows converting an enum's *value* straight to a pointer — and then segfaulted at runtime, since it wrote through a garbage address instead of into `op`'s own storage; this class of bug won't show up as a compile error, only as a crash.

### WAL record format
```
[magic_number]   uint32_t  4 bytes   (WAL_MAGIC_NUMBER)
[version_number] uint8_t   1 byte    (WAL_VERSION)
[index]          uint64_t  8 bytes   (sequence number, auto-incremented, never reset — even across truncate())
[operation]      uint8_t   1 byte    (enum class operation : uint8_t { set, del }, from constants.h)
[len_of_key]     uint32_t  4 bytes
[val_of_key]     char[]    variable
[len_of_data]    uint32_t  4 bytes
[val_of_data]    char[]    variable
[checksum]       uint32_t  4 bytes   (CRC32 over version→val_of_data)
```

### SSTable file format (binary, one file per flush, immutable once written)
```
[magic_number]   uint32_t  (SS_TABLE_MAGIC_NUMBER)
[version]        uint8_t   (SS_TABLE_VERSION)
[ss_table_index] uint64_t  (matches the number in the filename, e.g. ss_table_3.bin)
[entry_count]    uint64_t  (needed because entries are variable-length and a footer follows with no delimiter)
--- repeated entry_count times ---
[operation]      uint8_t   (set / del, same shared enum as WAL — del entries still write val_len=0/empty val)
[key_len]        uint32_t
[key]            char[]    variable
[val_len]        uint32_t
[val]            char[]    variable
--- end repeat ---
[checksum]       uint32_t  (CRC32 over version→last entry, including each entry's operation byte;
                            must read the WHOLE file before trusting anything — no early exit on match)
```
`get_value_from_ss_table()` returns `ss_table.h`'s `lookup_result` (`{lookup_status: not_found|tombstone|found, value}`), not a plain `std::optional` — see the tombstone/delete fix notes above for why a two-state optional isn't enough here.

### Manifest file format (`data/manifest.txt`, plain text, one value per line — deliberately NOT binary, since it's just a list of numbers)
```
MANIFEST_MAGIC_CONST   (string "MEWDB")
MANIFEST_VERSION       (uint32_t — NOT uint8_t, see gotcha below)
NEXT_SS_TABLE_INDEX    (uint64_t — tracked explicitly, NOT derived from the list below, because
                        compaction can later shrink the list's max without allowing index reuse)
NUM_SS_TABLES          (uint64_t — count of index lines that follow; informational only, the
                        actual read loop just reads to EOF rather than trusting this count)
SS_TABLE_INDEX_1
SS_TABLE_INDEX_2
...
```
Updates are never in-place edits: `add_new_ss_table_index()` rewrites the whole file to a temp file (`MANIFEST_TEMP_FILE_NAME`), flushes it, then atomically swaps it in via `std::filesystem::rename()`. No checksum — the write-then-rename mechanic already guarantees no half-written file is ever visible (a checksum would only add protection against *post-write* corruption, which was judged unnecessary for now).

### Key design decisions
**WAL / flush ordering (crash safety):** on every flush, the order is strictly: write SSTable → verify it → **update the manifest (durably)** → *then* truncate the WAL. Never truncate before the manifest is updated — if a crash happens in between, an un-recorded manifest state plus a wiped WAL would let the next flush silently overwrite a file that already held real data. Verified with an actual two-process restart test (no index collisions, correct resume).
**`db_engine` owns no SSTable-index state itself** — always asks `manifest_instance->get_next_ss_table_index()` (write) or `get_ss_table_indices()` (read/search), since the manifest is the single source of truth (avoids the class of bug where two places track the "same" fact and drift apart — hit multiple times this session with WAL/SSTable version constants and again with the manifest's own `num_ss_tables`).
**`GET` searches SSTables newest-to-oldest** (reverse iterator over the manifest's index list) once a key isn't in `curr_mem_table`, so an overwritten key resolves to its latest value rather than a stale one from an older file — and a tombstone found at any point stops the search immediately rather than falling through to older files.
**Error policy (deliberate, not an oversight):** a missing or corrupted SSTable encountered during `get()` throws and is left uncaught — crashes the whole REPL. Consistent with `manifest`'s constructor-time exceptions also being uncaught. Worth revisiting once there's a server process (Phase 8) where one bad lookup shouldn't take down everything.
**Old WAL/`wal` design decisions still hold:** `wal` owns its file, opens in constructor, closes in destructor; file created via temporary `ofstream` if missing; `db_engine` owns `wal*`/`manifest*` via pointer (forward declarations); `recover()` uses `db.recover_set()`/`recover_del()` (private, `friend class wal`); WAL write must succeed before MemTable update (durability).

### Concepts understood
- CRC32 checksums vs cryptographic hashes ✅
- Why WAL is append-only (sequential writes + crash safety) ✅
- Magic numbers and version bytes — including giving each file format (WAL/SSTable/manifest) its *own* independent magic number and version, since they can evolve independently ✅
- Fixed-width unsigned types (`uint8_t`, `uint32_t`, `uint64_t`) and their overflow risk — hit a real bug where an 8-bit SSTable index counter would've wrapped at 256 ✅
- Forward declarations and when to use pointers vs references; `friend class` for tightly coupled classes ✅
- `file.clear()` to reset stream error/EOF flags ✅
- Why WAL/SSTable files need checksums to detect crash-interrupted writes, and why atomic rename (write-to-temp-then-rename) is a *different*, often simpler technique for a file that gets fully rewritten each time (the manifest) rather than appended to ✅
- `std::fstream` buffers writes internally — only `close()`/destruction/explicit flush forces them to the OS; a leaked or never-closed write handle can silently lose all buffered data ✅ (found and fixed a real bug from this)
- `uint8_t` is `unsigned char`, which has a special "insert as character" `ostream` overload — streaming it to a text file writes a raw non-printable byte, not digit text; binary `.write()` calls are unaffected ✅ (found and fixed a real bug from this)
- Unsigned integer underflow — `i >= 0` can never be false for an unsigned loop counter, and decrementing an unsigned `0` wraps to a huge value instead of going negative ✅ (found and fixed a real crash from this)
- Why redundant/derivable state (a value that mirrors another piece of state instead of being computed from it) is a recurring source of bugs — hit repeatedly with duplicated `SET`/`DELETE` constants, `next_index` vs. the index list, and the manifest's `num_ss_tables` ✅
- LSM-tree "newer shadows older" read semantics — the same key can legitimately exist in multiple immutable SSTable files, so read order matters, and this is also *why* compaction (next phase) exists ✅
- RAII vs. manual `new`/`delete` for scoped objects (the `ss_table` inside `flush()` went from a leaked heap pointer to a local stack object) ✅
- Reading a file fully before trusting a whole-file checksum means "stop scanning once found" isn't compatible with checksum-verified reads ✅
- Tombstones — a delete needs to be durably *written down* (in memory as `nullopt`, on disk as an explicit marker), not just erased, or the fact that something was deleted can't survive being flushed/replayed; this is also why real deletes in an LSM-tree only get reclaimed later, during compaction ✅
- `std::optional` (or any two-state type) stops being enough once a caller needs to distinguish *three* things (e.g. "not found here" vs. "found, and it's a tombstone" vs. "found, here's the value") — collapsing two of those into the same `nullopt` silently breaks whichever logic depended on telling them apart ✅ (found and fixed a real regression from this, twice — once in-memory, once across SSTable files)
- `reinterpret_cast` legally allows converting an enum or integer *value* directly into a pointer — `reinterpret_cast<char*>(op)` (missing `&`) compiles cleanly with no warning, then segfaults at runtime because it treats `op`'s value as a memory address instead of taking `op`'s own address ✅ (found and fixed a real crash from this)

### Naming convention
- snake_case for all classes and variables (e.g. `db_engine`, `wal`, `ss_table`, `manifest`)
- Project headers use `#include "file.h"`, system headers use `#include <file>`
- Shared format constants (magic numbers, versions, file names) live centrally in `constants.h`, never duplicated per-file

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
├── main.cpp               ← full REPL, all commands wired, db_engine Db; (no-arg ctor)
├── calc.cpp               ← practice REPL (complete, not part of main build)
├── include/
│   ├── constants.h        ← all shared format constants + open_mode enum
│   ├── db_engine.h        ← db_engine class (owns wal*, manifest*)
│   ├── wal.h              ← wal class (write, recover, truncate)
│   ├── ss_table.h         ← ss_table class (write_to_ss_table, get_value_from_ss_table)
│   └── manifest.h         ← manifest class (durable SSTable index registry)
├── src/
│   ├── db_engine.cpp      ← db_engine implementation (curr_mem_table, tombstone-aware, + WAL + SSTable + manifest)
│   ├── wal.cpp            ← WAL implementation (write, recover, truncate, checksum)
│   ├── ss_table.cpp       ← SSTable implementation (write path + checksum-verified read path)
│   └── manifest.cpp       ← manifest implementation (load-or-create, write-then-atomic-rename)
├── practice/
│   ├── encoder.cpp        ← binary encoder/decoder practice (complete)
│   └── wal.cpp            ← standalone WAL write + recover practice (complete)
├── data/                  ← runtime output: wal, ss_table_N.bin, manifest.txt (wal is gitignored;
│                            manifest.txt/ss_table_*.bin are NOT yet — see note below)
└── build/                 ← generated by cmake (gitignored)
```
