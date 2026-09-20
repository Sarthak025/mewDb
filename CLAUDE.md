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
DELETE <key>         → OK | NOT_FOUND
EXISTS <key>         → TRUE | FALSE
KEYS                 → one key per line + END
RANGE <start> <end>  → sorted key-value pairs + END
PREFIX_SCAN <prefix> → matching key-value pairs + END
COMPACT              → COMPACTION SUCCESSFUL | ERROR: COMPACTION FAILED
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
| 5 | LSM Compaction | ✅ Done (core: full/manual compaction — auto-trigger + tiering deferred, see below) |
| 6 | Bloom filters + sparse index | ⬜ |
| 6a | Practice: standalone Bloom filter | ⬜ |
| 7 | Benchmark tool | ⬜ |
| 8 | TCP server + concurrency | ⬜ |
| 9 | Replication + consistent hashing | ⬜ |

---

## Current State (Phase 6 — Bloom filters + sparse index, not started)

**Phases 1–5 (core) complete**, including the tombstone/delete fix, full/manual LSM compaction, and exhaustive-test passes for both (see "Exhaustive test pass" sections below). All files building and tested. Full read/write/compact cycle (WAL → MemTable → SSTable flush → SSTable read-back → compaction → SSTable read-back, tombstone-aware throughout) works end-to-end and has been verified by actually building and running the project, not just reading the code. No known gaps remain in the core path. Deliberately deferred, not forgotten: automatic compaction triggering (currently `COMPACT` is manual-only) and a tiered/size-based compaction strategy — see "LSM Compaction" below for why these were scoped out of v1 and what would need to change to add them.

### Completed files
- `calc.cpp` — calculator REPL practice task
- `CMakeLists.txt` — C++17, sources: `main.cpp`, `src/db_engine.cpp`, `src/wal.cpp`, `src/ss_table.cpp`, `src/manifest.cpp`; links zlib (`-lz`)
- `main.cpp` — full REPL, all commands wired including `COMPACT`; `db_engine Db;` (no-arg constructor — filenames now come from `constants.h`); `EXIT`/`QUIT` comparison now trims surrounding whitespace via a small `trim()` helper (closes the item deferred at the end of Phase 4)
- `include/constants.h` — every shared format constant lives here: `WAL_VERSION`/`WAL_MAGIC_NUMBER`, `SS_TABLE_VERSION`/`SS_TABLE_MAGIC_NUMBER`, `MANIFEST_VERSION`/`MANIFEST_MAGIC_CONST`, `enum class operation : uint8_t { set, del }` (replaces the old loose `SET`/`DELETE` constants, shared by WAL/`db_engine`/`ss_table`), file name constants (`WAL_FILE_NAME`, `SS_TABLE_FILE_NAME` — renamed from `SS_TABLE_NAME`, `MANIFEST_FILE_NAME`, `MANIFEST_TEMP_FILE_NAME` — all under `data/`), `MEM_TABLE_SIZE_LIMIT`, and the `open_mode` enum (`read`/`write`) used by `ss_table`
- `include/db_engine.h` / `src/db_engine.cpp` — `db_engine` class; MemTable is `curr_mem_table` (renamed from `storage`), value type `std::optional<std::string>` (`nullopt` = tombstone); owns `wal*` and `manifest*` (no longer owns its own SSTable-index counter — always asks the manifest); both now constructed no-arg (`new wal()` / `new manifest()`, filenames read from `constants.h` internally rather than passed in — trades away constructor-injected test file paths, judged acceptable since this project's testing style has always been build-and-run against the real `data/` dir rather than isolated unit tests); `flush()` returns `bool`; public `exists()`/`get()`/`range()`/`prefix_scan()` all search SSTables (tombstone-aware), `exists_in_curr_mem_table()` is a private MemTable-only helper used internally by `set`/`del`/`recover_set`/`recover_del`; new `compact()` (see "LSM Compaction" below)
- `include/wal.h` / `src/wal.cpp` — adds `truncate()` (close → reopen via `std::ofstream` with `trunc` → reopen `in|app`); `index` never resets, even across truncation (LSN-style, for future replication)
- `include/ss_table.h` / `src/ss_table.cpp` — `write_to_ss_table()` (flush path, tombstone-aware); `read_ss_table()` does the one shared "open file, verify magic number, verify `ss_table_index` matches the filename, loop reading entries with per-read error checks, verify final checksum" pass and returns an in-memory `ss_table_data` — made **public** (was private) so `db_engine::compact()` can pull one file's raw `records` directly, since compaction needs the whole file's contents rather than a single-key/range/prefix view; `get_value_from_ss_table()` (single-key read, returns the 3-state `lookup_result`), `get_range_from_ss_table()`/`get_prefix_from_ss_table()`/`get_keys_from_ss_table()` (per-file multi-key read, returns tombstones included as `std::optional`-valued pairs) all still just call `read_ss_table()` once and operate on `data.records`. `open_mode::read`/`write` selects read vs. write for the constructor.
- `include/manifest.h` / `src/manifest.cpp` — durable registry of valid SSTable indices; constructed no-arg (see `db_engine` note above); gained `replace_ss_table_indices()` (see "LSM Compaction" below)
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

**Former known gap — now fully closed, as of 2026-09-07.** `EXISTS`, `RANGE`, and `PREFIX_SCAN` all search SSTables now, tombstone-aware, the same way `get()` does. (1) tombstone/delete fix at the MemTable level — done. (2) SSTable format extended to support tombstones — done. (3) `EXISTS`/`RANGE`/`PREFIX_SCAN` extended to search SSTables — done (see below for how). `db_engine::exists()` ended up a one-line delegate to `get()` (`return this->get(key).has_value();`) rather than its own search — `exists_in_curr_mem_table()` still exists as a private helper used internally by `set`/`del`/`recover_set`/`recover_del`, but is no longer part of the public surface.

**`RANGE`/`PREFIX_SCAN` SSTable search — completed 2026-09-07.** Harder than `GET`'s single-key case, because a range/prefix query can match *many* keys spread across the MemTable and every SSTable, and the same key can legitimately appear in more than one place. Design landed:
- `ss_table` gained `get_range_from_ss_tables(start, end)` / `get_prefix_from_ss_tables(prefix)` — each operates on *one file* (`this->ss_table_file`, no manifest, no loop over other files — mirrors `get_value_from_ss_table`'s scope), reads the whole file (still mandatory for the checksum), and returns every matching entry **including tombstones** as `std::vector<std::pair<std::string, std::optional<std::string>>>`. Deliberately 2-state (`optional`), not the 3-state `lookup_result` — every entry returned by definition matched and was found in this file, so there's no "not found" case to represent here, unlike the single-key lookup.
- `db_engine::range()`/`prefix_scan()` own the cross-file merge: start from a copy of `curr_mem_table` (the newest source), then walk the manifest's SSTable indices newest-to-oldest, merging each file's matches in with **"insert only if not already present"** — so the first (newest) source to mention a key wins and every later, older duplicate for that key is ignored. Tombstones are kept all the way through this merge (so they can correctly block stale older values for the same key) and only filtered out in the final pass that builds the returned vector.
- Materializing a whole matched-and-checksum-verified SSTable file into a temp map per file (rather than a fully lazy streaming merge) was a deliberate, examined choice — every file is bounded by `MEM_TABLE_SIZE_LIMIT` today, so the cost is negligible now; a true merge-iterator (never materializing a whole file, pulling from each sorted source lazily) is the eventual "correct" production answer but was judged unnecessary until `MEM_TABLE_SIZE_LIMIT` or file count actually grows enough to matter — revisit if either changes materially.
- Bug hit and fixed, **twice, in two different places** — worth remembering as a pattern: an "insert/overwrite only if the key is *already* present" merge condition (backwards — it should be "insert only if *not* already present") first appeared in an early `ss_table`-side merge attempt (confirmed via test: returned an empty result unconditionally), got fixed there, and then the *identical* inverted condition reappeared independently in `db_engine::range()`/`prefix_scan()`'s own cross-source merge once that logic moved up a layer. Both manifestations were confirmed by actually running the REPL: a key with both a stale flushed value and a current live value returned the *stale* one; a key that existed only in a flushed SSTable didn't appear in results at all. Same fix both times: invert `!=` to `==`.

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
**Compaction ordering (crash safety, same principle as flush) — see "LSM Compaction" below for full detail:** write the merged SSTable → verify it → **update the manifest (durably, `replace_ss_table_indices()`)** → *then* delete the old physical files. `compact()` never touches the WAL or `curr_mem_table` at all — those protect *not-yet-durable* writes, and compaction only ever operates on data that's already durable on disk; the two are independent durability domains that never need to interact.

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
- LSM-tree "newer shadows older" read semantics — the same key can legitimately exist in multiple immutable SSTable files, so read order matters, and this is also *why* compaction exists ✅
- RAII vs. manual `new`/`delete` for scoped objects (the `ss_table` inside `flush()` went from a leaked heap pointer to a local stack object) ✅
- Reading a file fully before trusting a whole-file checksum means "stop scanning once found" isn't compatible with checksum-verified reads ✅
- Tombstones — a delete needs to be durably *written down* (in memory as `nullopt`, on disk as an explicit marker), not just erased, or the fact that something was deleted can't survive being flushed/replayed; this is also why real deletes in an LSM-tree only get reclaimed later, during compaction ✅
- `std::optional` (or any two-state type) stops being enough once a caller needs to distinguish *three* things (e.g. "not found here" vs. "found, and it's a tombstone" vs. "found, here's the value") — collapsing two of those into the same `nullopt` silently breaks whichever logic depended on telling them apart ✅ (found and fixed a real regression from this, twice — once in-memory, once across SSTable files)
- `reinterpret_cast` legally allows converting an enum or integer *value* directly into a pointer — `reinterpret_cast<char*>(op)` (missing `&`) compiles cleanly with no warning, then segfaults at runtime because it treats `op`'s value as a memory address instead of taking `op`'s own address ✅ (found and fixed a real crash from this)
- Erasing an element from a container while a range-based `for` loop (or any loop reusing the same iterator) is iterating over it is undefined behavior — the loop's implicit increment runs on an iterator invalidated by the erase. Compiles cleanly, can even appear to work by luck, but isn't reliable — the safe idiom for an associative container is an explicit iterator loop using `it = container.erase(it)`'s return value in place of a separate increment ✅ (found and fixed a real crash — an actual segfault — from this)
- A monotonically-increasing counter (like the manifest's `next_ss_table_index`) lets you *prove* properties like "a freshly-issued value is always greater than every value issued before it" — useful for reasoning, but also a trap: a plausible-looking fix built on the wrong version of that fact (e.g. sorting by *value* to solve what is actually a *positional* ordering problem) can be a complete no-op without being obviously wrong ✅
- Why compaction never needs to touch the WAL or MemTable: they protect exactly one kind of state (writes acknowledged but not yet durable as an SSTable), and compaction only ever operates on data that's already durable — two independent durability domains that never overlap, each with its own separate crash-safety mechanism (a replayable log for one, atomic durable-swap-then-delete for the other) ✅
- Dropping a tombstone is only safe once nothing *older* remains that it could still be shadowing — precisely, only when the operation touching it spans every SSTable up to the oldest one that currently exists. Full compaction satisfies this trivially (it always touches everything); a partial/tiered compaction would need to actually check it ✅

### Naming convention
- **PascalCase for all types** — classes, structs, and enums (e.g. `DbEngine`, `Wal`, `SsTable`, `Manifest`, `SsTableData`, `WalData`, `Entry`, `LookupResult`, `LookupStatus`, `Operation`, `OpenMode`)
- **snake_case for everything else** — variables, member variables, methods, and free functions (e.g. `wal_instance`, `curr_mem_table`, `ss_table_index`, `write_to_ss_table()`, `get_ss_table_indices()`)
- SCREAMING_SNAKE_CASE for constants in `constants.h` (e.g. `WAL_MAGIC_NUMBER`, `MEM_TABLE_SIZE_LIMIT`)
- File names stay snake_case and are independent of the type they hold (`SsTable` lives in `ss_table.h` / `ss_table.cpp`)
- Switched from all-snake_case on 2026-09-20; older prose elsewhere in this file may still spell types the old way (`db_engine`, `ss_table`, …) — the code is the source of truth
- Project headers use `#include "file.h"`, system headers use `#include <file>`
- Shared format constants (magic numbers, versions, file names) live centrally in `constants.h`, never duplicated per-file

### Exhaustive test pass — completed 2026-09-08, closes out Phase 4

Before moving to Phase 5, ran a full adversarial test pass covering every command (`SET`/`GET`/`DELETE`/`EXISTS`/`KEYS`/`RANGE`/`PREFIX_SCAN`) and every edge case (empty DB, restarts, flush boundaries, torn writes), plus an architecture review. Found and fixed:

1. **`KEYS` never searched SSTables** — only ever looked at `curr_mem_table`, so any key that had been flushed and evicted from memory silently disappeared from `KEYS` output even though `GET` could still find it. Fixed by giving `keys()` the same newest-to-oldest SSTable merge pattern already used by `range()`/`prefix_scan()`, via the new `get_keys_from_ss_table()` method (also reused by `del()` to check whether a key exists anywhere before returning `OK`/`NOT_FOUND`).
2. **Two critical regressions in `wal::recover()`**, both introduced while trying to make WAL's error handling "consistent" with SSTable's throw-on-corruption policy, both caught by testing before being merged:
   - The magic-number check (the loop's normal end-of-file / end-of-replay signal) was changed from `break` to `throw` — this crashed the program on **every single startup**, including a fresh empty database, because reaching EOF was being treated as corruption.
   - The checksum-mismatch check was also changed from graceful-stop to `throw` — a checksum mismatch on the *last* record is the expected signature of a crash mid-write; throwing here would have permanently bricked the database after any ordinary crash, since every subsequent restart would hit the same torn record and refuse to start.
   - **The lesson (general, not just for this bug):** don't reuse the same detection signal for both "normal loop termination" and "genuine corruption" without splitting the cases. For an *immutable, already-flushed* file (SSTable), any read failure really is corruption — throwing is correct. For a *replayed, append-only* log (WAL), reaching EOF and a torn trailing record are both **expected, routine outcomes** of replay, not failures — recovery must stop gracefully, not crash. Both regressions were reverted back to `break`, confirmed via rebuild + targeted tests (fresh empty-DB startup exits 0; a WAL truncated mid-last-record correctly recovers the earlier good entries, prints "Recovery stopped", and exits 0).
3. Other fixes verified in the same pass: the REPL no longer busy-loops forever when stdin hits EOF without an `EXIT`/`QUIT` (`if (!std::cin) break;` added right after `getline`); `checkForArguments()`'s error message no longer says "Not enough arguments" for a too-many-arguments case.
4. Architecture cleanup done in the same pass: `ss_table`'s three read methods deduped into one shared `read_ss_table()` (see above); `ss_table_index` is now validated against the SSTable's own filename on every read; naming fixed to consistently singular (`get_range_from_ss_table`, not `_ss_tables`); `.gitignore` now covers `data/` wholesale instead of a stale bare `wal` pattern. Deferred to Phase 5 (compaction's own concern): a `manifest::replace_ss_table_indices()` API for atomically swapping many old SSTable indices for one compacted one — the manifest stays append-only (`add_new_ss_table_index`) until then.

All of the above were verified by actually rebuilding and running the project — piped stdin scenarios, simulated torn writes, full restart cycles — not by reading the code alone.

### Open item before Phase 5 — resolved 2026-09-14
**`EXIT`/`QUIT` not trimming surrounding whitespace** before comparison was fixed alongside the compaction work below: a small `trim()` helper in `main.cpp` is applied to just the exit/quit comparison, leaving the `SET`-value tokenizer untouched. Confirmed working.

---

## LSM Compaction — completed 2026-09-14, closes out Phase 5's core

**The problem this solves:** every `flush()` creates a new immutable SSTable file, and nothing ever removed one — overwritten and tombstoned keys just sat on disk forever, and every read had to search a strictly growing list of files. Compaction reclaims that space and shrinks the search space by merging multiple SSTables into one.

**Design landed (v1 — deliberately the simplest correct version, not the eventual production shape):**
- **Full/major compaction only, manually triggered.** A new `COMPACT` REPL command always merges *every* SSTable currently in the manifest into a single new file — no size tiers, no automatic trigger policy. This was a deliberate staging choice: it isolates the genuinely hard parts (the merge, the tombstone-drop rule, the atomic manifest swap) from "when should this run," which is a separate, independently-addable concern. Automatic triggering (e.g. a threshold check at the end of `flush()`) and a real tiered/size-based strategy are both explicitly deferred, not forgotten — revisit once the core is trusted.
- **`manifest::replace_ss_table_indices(const std::vector<uint64_t> &old_indices, uint64_t new_index)`** — the API deferred from Phase 4. Same shape as `add_new_ss_table_index()` (rewrite to a temp file, flush, verify, `std::filesystem::rename()` over the real manifest), but computes the new index list as `curr_indices − old_indices` (a hand-rolled sorted-vector set difference, `subtract_vectors()`) plus `new_index`, rather than only ever appending. `num_ss_tables` is derived from the resulting list's `size()` (not hand-incremented — deliberately avoiding the exact "redundant tracked count drifts from reality" bug class already hit once with this same field), and `next_ss_table_index = new_index + 1` is updated as part of the same atomic step, since it's a private member only a `manifest` method can touch.
- **`db_engine::compact()`** — no-ops (returns `true` immediately) when fewer than 2 SSTables exist, since there's nothing to gain. Otherwise: reads every current SSTable's full contents via (now-public) `read_ss_table()`, merges newest-to-oldest into one `std::map<std::string, std::optional<std::string>>` with the same "insert only if not already present" pattern used by `range()`/`prefix_scan()`/`keys()`, then **unconditionally drops every tombstone** from the merged result before writing — safe specifically *because* this is always a full compaction spanning every existing SSTable, so by construction nothing older can possibly survive to be resurrected by removing the tombstone that was shadowing it. Writes the merged map via the existing `write_to_ss_table()`, calls `replace_ss_table_indices()` (checking its return value — a failure here must *not* be followed by deleting the old files), and only then deletes the old physical SSTable files. Ordering mirrors `flush()`'s established "durable state change before destroying old state" principle exactly.
- **The tombstone-drop rule, precisely stated** (matters the moment partial/tiered compaction is ever added): dropping a tombstone is only safe when the compaction set includes every SSTable older than it — i.e. reaches all the way to the oldest SSTable that currently exists. Full compaction satisfies this trivially, every time, since it always includes everything. A future partial compaction (e.g. size-tiered, merging only some of the oldest files) would need to actually check this before dropping a tombstone, or carry it forward unchanged if the check fails.
- **A known, currently-harmless simplification to revisit if tiering is ever added:** `replace_ss_table_indices()` places `new_index` at the *end* of the index list (`push_back`), which the manifest's consumers treat positionally as "newest." That's correct for v1 only because `old_indices` is always the *entire* list, so the result always has exactly one entry — position is moot. A compacted file that replaces only the *oldest* N files (tiered compaction) would need to be positioned where the oldest of those N used to sit, not at the newest end, or `get()`/`range()`/etc.'s newest-to-oldest reverse search would incorrectly treat old, just-compacted data as more recent than genuinely newer untouched files.

### Bugs found and fixed during compaction's implementation

Several real bugs surfaced during review and testing — worth keeping as a reference, same as the Phase 4 list below:
1. `manifest.cpp`: `std::fstream temp_manifest_file.open(...)` — invalid C++ (can't declare a variable and chain `.open()` via `.` in one statement); wouldn't compile. Fixed to a proper constructor call, matching `add_new_ss_table_index()`'s existing pattern.
2. `manifest.cpp`: the new temp-file write path initially had no `.flush()` and no write-success check before renaming — inconsistent with `add_new_ss_table_index()`'s existing defensive pattern. Added both.
3. `subtract_vectors()` (the sorted-vector set-difference helper): the first version's merge loop only handled the "equal" and "`curr[i] < remove[j]`" cases, silently doing nothing (advancing neither pointer) when `curr[i] > remove[j]` — an **infinite loop** whenever `remove` contained any value not present in `curr`. It also unconditionally read `curr[0]` in a leading loop even when `curr` was empty (out-of-bounds). Fixed with the correct three-way branch (`==` / `<` / `else`), which also fixed the empty-`curr` case for free since the loop condition now checks bounds before indexing.
4. A `std::lower_bound` + `insert` was added at one point to place `new_index` in sorted-by-value order, aimed at the positional-ordering concern above — but it's provably a no-op: since `new_index` always comes from the manifest's monotonically-increasing counter, it is *always* greater than every index already in the list, so `lower_bound` always returns `end()`. Reverted to a plain `push_back`. (The real fix for positional ordering, when it's eventually needed, is a different mechanism entirely — inserting at a *position*, not sorting by *value* — see the note above.)
5. `db_engine::compact()`: `replace_ss_table_indices()`'s `bool` return was initially ignored — if that call failed, the code would still proceed to delete the old physical files, leaving the manifest pointing at files that no longer exist. Fixed to check-and-bail, matching `flush()`'s existing pattern of checking every step.
6. `db_engine::compact()`: the merged `ss_table` was initially heap-allocated (`new ss_table(...)`) with `delete` only reached on the success path — an early `return false` on `write_to_ss_table()` failure skipped the `delete`, leaking the object and its still-open file handle. Same class of bug already found and fixed once in `flush()` (see "Concepts understood"). Fixed the same way: switched to a stack-allocated local object (RAII).
7. **The tombstone-drop loop originally erased from `full_data` (a `std::map`) while iterating it with a range-based `for` loop** — undefined behavior, since erasing the element the loop's hidden iterator currently points at invalidates that iterator before the loop's implicit increment runs. This wasn't just theoretical: reproduced as an actual **segfault (exit code 139)** via a concrete repro (two flushes, the second containing a tombstone, then `COMPACT`); a control run with the same command sequence but no deletes succeeded cleanly, isolating the crash to this exact loop. Fixed with the correct idiom — an explicit iterator loop that only advances via `it = full_data.erase(it)`'s return value, advancing manually only when *not* erasing. Rebuilt and re-ran the exact crash repro to confirm: exit 0, and confirmed via exact byte-count on the resulting SSTable file that the tombstone was genuinely dropped, not merely surviving-but-hidden.

### Exhaustive test pass — completed 2026-09-14, closes out Phase 5's core

Before moving to Phase 6, ran a full pass covering compaction's interaction with the rest of the system (not just the happy path already covered while fixing the bugs above), all via real builds and real runs:
1. `COMPACT` run twice in a row — second call correctly no-ops (the `< 2` guard) once only one SSTable remains; no new file created, manifest unchanged.
2. `COMPACT` with exactly 2 SSTables, no tombstones — merges cleanly, `KEYS` correct.
3. The same key `SET` across 3 separately-flushed files — after `COMPACT`, `GET` resolves to the newest write and `KEYS` lists the key exactly once (no duplicate/stale survivors).
4. `RANGE`/`PREFIX_SCAN`/`EXISTS` immediately after a `COMPACT` — all correct; these have their own SSTable-merge code paths, distinct from `GET`'s, and hadn't been exercised by any of the fixes above.
5. Live, unflushed `curr_mem_table` data present during a `COMPACT` — confirmed untouched, including surviving a full process restart afterward (WAL replay recovered it), proving `compact()` never touches the WAL or in-memory state, only already-flushed SSTables.
6. `SET` → flush → `DELETE` → flush (tombstone) → `SET` a new value → flush → `COMPACT`, all for the same key — the final value wins correctly even with a tombstone sandwiched in the middle of the key's history; confirmed via exact byte-count that no leftover tombstone record survived in the compacted file.

No further bugs found. `compact()` is considered correct and closes out Phase 5's core.

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
├── main.cpp               ← full REPL, all commands wired incl. COMPACT, db_engine Db; (no-arg ctor), trim() for EXIT/QUIT
├── calc.cpp               ← practice REPL (complete, not part of main build)
├── include/
│   ├── constants.h        ← all shared format constants + open_mode enum
│   ├── db_engine.h        ← db_engine class (owns wal*, manifest*; compact())
│   ├── wal.h              ← wal class (write, recover, truncate; no-arg ctor)
│   ├── ss_table.h         ← ss_table class (write_to_ss_table, get_value_from_ss_table, public read_ss_table)
│   └── manifest.h         ← manifest class (durable SSTable index registry; replace_ss_table_indices)
├── src/
│   ├── db_engine.cpp      ← db_engine implementation (curr_mem_table, tombstone-aware, + WAL + SSTable + manifest + compact)
│   ├── wal.cpp            ← WAL implementation (write, recover, truncate, checksum)
│   ├── ss_table.cpp       ← SSTable implementation (write path + checksum-verified read path)
│   └── manifest.cpp       ← manifest implementation (load-or-create, write-then-atomic-rename, replace_ss_table_indices)
├── practice/
│   ├── encoder.cpp        ← binary encoder/decoder practice (complete)
│   └── wal.cpp            ← standalone WAL write + recover practice (complete)
├── data/                  ← runtime output: wal, ss_table_N.bin, manifest.txt (wal is gitignored;
│                            manifest.txt/ss_table_*.bin are NOT yet — see note below)
└── build/                 ← generated by cmake (gitignored)
```
