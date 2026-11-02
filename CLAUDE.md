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
| 6 | Bloom filters + sparse index | ✅ Done (core: dynamic per-file Bloom filter + sparse index, new block-based SSTable v2 format — see below) |
| 6a | Practice: standalone Bloom filter | ✅ Done |
| 7 | Benchmark tool | 🔄 In progress — design underway; currently on a WAL-durability detour (see "Phase 7" below) |
| 8 | TCP server + concurrency | ⬜ |
| 9 | Replication + consistent hashing | ⬜ |

---

## Current State (Phase 7 in progress — benchmark design + WAL durability detour; SSTable format v2)

**Phase 7 in progress (as of 2026-10-30).** Benchmark design decisions are mostly made (in-process against `DbEngine`, `steady_clock`, percentile reporting — see "Phase 7 — Benchmark Tool" below), but no benchmark code exists yet. While researching `fsync` for the benchmark, a `kill -9` test proved acknowledged `SET`s could be lost (WAL records sat in the user-space stream buffer), which turned into a durability detour: the WAL was rewritten on `FILE*` with a per-record flush + `F_FULLFSYNC`, a failure latch, and point-in-time crash recovery with torn-tail repair. The WAL was then bumped to v2 (header checksum, so a corrupted length is never mistaken for a torn tail), and the flush/compaction path was made durable too (commit `a054af1`: SSTables, the manifest, and the `data/` directory are all synced, in order, before the WAL is truncated or old files are deleted). Still queued: the `Options` struct (configurable data dir + MemTable limit + sync level), then the benchmark itself. Full detail, decisions, and the open-items list are in the "Phase 7" section below.

**Phases 1–6 (core) complete.** Phase 6 replaced the entire SSTable file format (v1 → v2: a single flat entry list with one whole-file checksum → a block-based format with a Bloom filter and a sparse index, see "Bloom Filters + Sparse Index" below) and closed out the two read-cost problems that motivated it: a point lookup no longer needs to read+checksum a whole file just to learn a key isn't in it (Bloom filter skip-check), and a point lookup that does need to read a file only reads *one* ~10-record block, not the whole thing (sparse index). Full read/write/compact cycle (WAL → MemTable → SSTable v2 flush → SSTable v2 read-back → compaction → SSTable v2 read-back, tombstone-aware throughout) works end-to-end and has been verified by actually building and running the project — including a ~2,000-command adversarial pass and a two-process WAL-recovery test (see "Exhaustive test pass" under Phase 6 below — note that test only covered a *clean* shutdown, see Phase 7). No known gaps remained in the core path at the end of Phase 6 — Phase 7's durability review has since found several, see "Phase 7" below. Deliberately deferred, not forgotten (unchanged since Phase 5): automatic compaction triggering (currently `COMPACT` is manual-only) and a tiered/size-based compaction strategy — see "LSM Compaction" below for why these were scoped out of v1 and what would need to change to add them.

The full byte-level SSTable v2 schema lives in **`ss_table_schema.md`** (project root) — that file is the authoritative, up-to-date reference for the on-disk format; this file summarizes the *decisions and reasoning* behind it, not the byte layout itself, to avoid the two documents drifting apart.

### Completed files
- `calc.cpp` — calculator REPL practice task
- `CMakeLists.txt` — C++17, sources: `main.cpp`, `src/db_engine.cpp`, `src/wal.cpp`, `src/ss_table.cpp`, `src/manifest.cpp`, `src/file_io.cpp`; links zlib (`-lz`) and xxHash (`find_package(xxHash CONFIG REQUIRED)`, `xxHash::xxhash` — needed for the Bloom filter's hashing, though `XXH_INLINE_ALL` means it's actually header-only)
- `ss_table_schema.md` — authoritative byte-level SSTable v2 schema (Header/Data Blocks/Bloom Filter Block/Sparse Index Block/Footer); written collaboratively and kept current as the format was implemented — check this, not this file, for exact field layouts
- `main.cpp` — full REPL, all commands wired including `COMPACT`; `DbEngine Db;` (no-arg constructor — filenames now come from `constants.h`); `EXIT`/`QUIT` comparison now trims surrounding whitespace via a small `trim()` helper (closes the item deferred at the end of Phase 4)
- `include/constants.h` — every shared format constant lives here: `WAL_VERSION`/`WAL_MAGIC_NUMBER`, `SS_TABLE_VERSION`/`SS_TABLE_MAGIC_NUMBER` (version bumped `1`→`2` for the block-based format), `MANIFEST_VERSION`/`MANIFEST_MAGIC_CONST`, `enum class Operation : uint8_t { set, del }` (replaces the old loose `SET`/`DELETE` constants, shared by WAL/`DbEngine`/`SsTable`), file name constants (`WAL_FILE_NAME`, `SS_TABLE_FILE_NAME`, `MANIFEST_FILE_NAME`, `MANIFEST_TEMP_FILE_NAME` — all under `data/`), `MEM_TABLE_SIZE_LIMIT`, the `OpenMode` enum (`read`/`write`) used by `SsTable`, and the new v2-format constants: `RECORDS_PER_BLOCK` (10), `BYTE_SIZE` (8, used for bit/byte conversions), `FOOTER_SIZE` (20), `BLOOM_FILTER_TARGET_FP_RATE` (0.001 — must be `double`, not an integer type, see "Bugs found" below), `BLOOM_FILTER_SEED_1`/`BLOOM_FILTER_SEED_2` (6969/6767, the two xxHash seeds)
- `include/db_engine.h` / `src/db_engine.cpp` — `DbEngine` class; MemTable is `curr_mem_table` (renamed from `storage`), value type `std::optional<std::string>` (`nullopt` = tombstone); owns `Wal*` and `Manifest*` (no longer owns its own SSTable-index counter — always asks the manifest); both now constructed no-arg (`new Wal()` / `new Manifest()`, filenames read from `constants.h` internally rather than passed in — trades away constructor-injected test file paths, judged acceptable since this project's testing style has always been build-and-run against the real `data/` dir rather than isolated unit tests); `flush()` returns `bool`; public `exists()`/`get()`/`range()`/`prefix_scan()` all search SSTables (tombstone-aware), `exists_in_curr_mem_table()` is a private MemTable-only helper used internally by `set`/`del`/`recover_set`/`recover_del`; `compact()` (see "LSM Compaction" below) — none of `db_engine.cpp`'s own logic needed to change for the v2 format, since it only ever calls `SsTable`'s public methods, which absorbed the format change entirely. As of Phase 7, `set()`/`del()` **throw** if the WAL write/sync fails (previously a failed WAL write silently skipped the MemTable update and the REPL still printed `OK`). The "no injectable paths" trade-off above is being reversed by the planned `Options` struct (see Phase 7)
- `include/wal.h` / `src/wal.cpp` — **rewritten in Phase 7** (see "Phase 7 — WAL durability detour" below): file handle is now `std::unique_ptr<std::FILE, decltype(&std::fclose)>` (opened `"a+b"`, constructor throws `std::system_error` if it can't open — e.g. `data/` missing), not `std::fstream`; every `write()` does `fwrite` × 10 fields → `fflush` → `fcntl(fd, F_FULLFSYNC)` (macOS; `fsync` on other Unix; `_commit` on Windows — the Windows branches have never been compiled), with an `EINTR` retry loop; a `write_failed` latch makes every later write fail after any write/sync error (fsyncgate reasoning); `recover()` does point-in-time recovery, truncates (`ftruncate` + sync) any invalid tail back to the last valid record, and reports what it discarded on stderr; `truncate()` reopens `"w+b"`, syncs, reopens `"a+b"`. `index` is meant to never reset (LSN-style, for future replication) — but see the known gap in Phase 7: it *does* reset to 0 on a restart that follows a flush. WAL byte format bumped to v2 (header checksum, see "WAL record format" below; still independent of SSTable format)
- `include/ss_table.h` / `src/ss_table.cpp` — the file that absorbed the entire v2 rewrite. See "Bloom Filters + Sparse Index" below for the full design and bug history. Public surface unchanged in shape from Phase 4/5 (`write_to_ss_table()`, `get_value_from_ss_table()`, `get_range_from_ss_table()`, `get_prefix_from_ss_table()`, `get_keys_from_ss_table()`, `read_ss_table()`) — callers in `db_engine.cpp` did not need to change at all, only the internals did. New free functions (block/footer/Bloom-filter/sparse-index read+write helpers, `insert_to_bloom_filter()`/`is_present_in_bloom_filter()`) live at file scope in `ss_table.cpp`, not as class methods, mirroring how `key_val_checksum()` already worked. Phase 7: write mode now uses a `FILE*` (`unique_ptr` + `fclose`) opened `"wb"` (truncate, not append — a retried flush to the same index used to append onto a partial file), every `write_*_block()` helper returns `bool` and every write is checked, and `write_to_ss_table()` ends with `sync_file()` + `sync_parent_directory()`. Read mode still uses `std::fstream`
- `include/manifest.h` / `src/manifest.cpp` — durable registry of valid SSTable indices; constructed no-arg (see `DbEngine` note above); gained `replace_ss_table_indices()` (see "LSM Compaction" below); untouched by the Phase 6 format change. Phase 7: all three writers (fresh create, `add_new_ss_table_index()`, `replace_ss_table_indices()`) now go through one private `write_manifest(next_index, indices)` — builds the whole text in a string, writes the temp file via `FILE*`, syncs it, closes, `rename()`s (non-throwing `error_code` overload) over `manifest_file_name`, then syncs the directory; in-memory `next_ss_table_index`/`num_ss_tables` are updated only *after* the rename succeeds (previously mutated before the write, so a failed write left memory and disk disagreeing). This also removed the old member-vs-`MANIFEST_FILE_NAME` constant mix-up in the rename path
- `include/file_io.h` / `src/file_io.cpp` — **new in Phase 7**: shared durability helpers extracted from `wal.cpp` — `write_bytes()`, `file_position()`, `sync_file()` (`fflush` + `F_FULLFSYNC`/`fsync`, `EINTR`-retried), `sync_parent_directory()` (opens the directory read-only and syncs that descriptor, preserving `errno` across `close`; no-op on Windows), and the `FileOffset` alias. Used by `Wal`, `SsTable` (write mode), and `Manifest`
- `practice/encoder.cpp` — binary encoder/decoder practice (complete)
- `practice/wal.cpp` — standalone WAL write + recover practice (complete)
- `practice/bloom_filter.cpp` — standalone Bloom filter practice (6a, complete) — see "Bloom Filters + Sparse Index" below for the design and the measured false-positive-rate investigation
- `practice/chrono.cpp` — Phase 7 timing practice (`std::chrono`); currently uses `high_resolution_clock` (practice only — the benchmark will use `steady_clock`)
- `tests/` — **gitignored, not part of the repo.** The user's decision: the repo is only for the database itself; testing is Claude's job, and Claude writes/maintains tests here. Currently `tests/wal_recovery_test.py` (WAL v2 byte format, torn-tail vs corruption classification, point-in-time recovery, a real `kill -9` crash test), `tests/command_surface_test.py` (every command across several flushes, a restart with WAL replay, and `COMPACT`), and `tests/crash_stress_test.py` (randomized: pipes batches of SET/DELETE/COMPACT, `SIGKILL`s at a random moment — often mid-flush or mid-compaction — restarts, checks every acknowledged write survived and recovery never reports corruption; `MEWDB_CRASH_TRIALS` / `MEWDB_CRASH_SEED` env vars). Run each as `python3 tests/<file> <path-to-mewDb-binary>` — each test runs the binary with `cwd` set to a fresh temp dir, so the real `data/` is never touched. Verify against a Release build made in the scratchpad from the repo's sources, not the user's `build/`

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

### WAL record format (v2, `WAL_VERSION = 2`)
```
--- fixed-size header (26 bytes) ---
[magic_number]   uint32_t  4 bytes   (WAL_MAGIC_NUMBER)
[version_number] uint8_t   1 byte    (WAL_VERSION)
[index]          uint64_t  8 bytes   (sequence number, auto-incremented; survives truncate() in-process,
                                      but resets to 0 on a restart after a flush — known gap, see Phase 7)
[operation]      uint8_t   1 byte    (enum class operation : uint8_t { set, del }, from constants.h)
[key_len]        uint32_t  4 bytes
[val_len]        uint32_t  4 bytes
[header_crc]     uint32_t  4 bytes   (CRC32 over version→val_len; magic excluded)
--- variable-size payload ---
[key]            char[]    key_len bytes
[val]            char[]    val_len bytes
[record_crc]     uint32_t  4 bytes   (CRC32 over version→val, computed by continuing from header_crc)
```
One checksum pair **per record** — there is no whole-file checksum. Each record is verified independently, so a bad record does *not* make later records' checksums fail (relevant to the recovery-policy discussion in Phase 7). v1 (Phase 3–7) had one checksum at the end and interleaved each length with its bytes (`key_len, key, val_len, val, checksum`) — recovery had to *use* a length to read before anything could verify it. v2 moved both lengths into a fixed header with its own checksum, so a length is verified before it is trusted (same rule as SSTable v2's checksummed header protecting `entry_count`). No v1 reader exists: a v1 record fails the version check and is discarded as corrupt.

### SSTable file format — **superseded, see `ss_table_schema.md` for the current (v2) format**
The format described in this section (a single flat entry list behind one whole-file checksum, `SS_TABLE_VERSION = 1`) was replaced during Phase 6 by a block-based format (`SS_TABLE_VERSION = 2`) with a Bloom filter and a sparse index — see "Bloom Filters + Sparse Index" below for why, and `ss_table_schema.md` for the exact byte layout. Kept here, collapsed, only as a historical note of what v1 looked like:
```
[magic_number][version][ss_table_index][entry_count]
--- repeated entry_count times: [operation][key_len][key][val_len][val] ---
[checksum]   (CRC32 over version→last entry; whole file read before trusting anything)
```
`get_value_from_ss_table()` returns `ss_table.h`'s `LookupResult` (`{status: not_found|tombstone|found, value}`), not a plain `std::optional` — see the tombstone/delete fix notes above for why a two-state optional isn't enough here. This 3-state shape carried over unchanged into v2.

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
**WAL / flush ordering (crash safety):** on every flush, the order is strictly: write SSTable → **sync it + sync `data/`** → write temp manifest → sync it → `rename()` → sync `data/` → *then* truncate the WAL (which syncs too). There is no separate "verify" step — older versions of this file claimed one, but `flush()` never had it; durability comes from the syncs. Never truncate before the manifest is durably updated. Never truncate before the manifest is updated — if a crash happens in between, an un-recorded manifest state plus a wiped WAL would let the next flush silently overwrite a file that already held real data. Verified with an actual two-process restart test (no index collisions, correct resume).
**`db_engine` owns no SSTable-index state itself** — always asks `manifest_instance->get_next_ss_table_index()` (write) or `get_ss_table_indices()` (read/search), since the manifest is the single source of truth (avoids the class of bug where two places track the "same" fact and drift apart — hit multiple times this session with WAL/SSTable version constants and again with the manifest's own `num_ss_tables`).
**`GET` searches SSTables newest-to-oldest** (reverse iterator over the manifest's index list) once a key isn't in `curr_mem_table`, so an overwritten key resolves to its latest value rather than a stale one from an older file — and a tombstone found at any point stops the search immediately rather than falling through to older files.
**Error policy (deliberate, not an oversight):** a missing or corrupted SSTable encountered during `get()` throws and is left uncaught — crashes the whole REPL. Consistent with `manifest`'s constructor-time exceptions also being uncaught. Worth revisiting once there's a server process (Phase 8) where one bad lookup shouldn't take down everything.
**WAL/`Wal` design decisions (updated Phase 7):** `Wal` owns its file — opened in the constructor (`fopen "a+b"`, which also creates it if missing; throws if it can't), closed automatically by the `unique_ptr`'s `fclose` deleter (no hand-written destructor); `DbEngine` owns `Wal*`/`Manifest*` via pointer (forward declarations); `recover()` uses `db.recover_set()`/`recover_del()` (private, `friend class Wal`); WAL write **and sync** must succeed before the MemTable is updated, and `set()`/`del()` throw if they don't (durability — `OK` must never be printed for a write that isn't on disk).
**Durability gap — closed 2026-10-31 (commit `a054af1`).** The flush path used to be less durable than the WAL it empties: the SSTable's *tail* sat in `fstream`'s user-space buffer until the `SsTable` destructor ran at the end of `flush()` — *after* `Wal::truncate()` — and nothing was ever `fsync`ed. Precisely (measured with lldb on `304b361`, 72-entry SSTable): each `tellp()` in `write_to_ss_table()` forces the buffer out (libc++ `tellp` → `seekoff` → `sync()` → `fwrite` + `fflush`), and the last `tellp()` is the one taken just before the sparse index — so header + data blocks + Bloom filter (1923 bytes) reached the OS during the write, while the sparse index + footer (the last 176 of 2099 bytes) were still in the process when the manifest rename and WAL truncate happened. `write_to_ss_table()` returning `good()` only meant "the stream accepted the bytes", not "they're in the file". Not just a power-loss problem: `tests/crash_stress_test.py` run against `304b361` hit it with a plain `SIGKILL` mid-flush ⇒ next startup aborted with `corrupted footer` (the manifest listed an SSTable whose last 20 bytes were Bloom filter bytes, not a footer). Against `a054af1`: 360 random kills across 6 seeds, zero failures. Still unsynced (accepted, harmless): `compact()`'s deletion of old SSTable files — after a power loss they may reappear as orphans not listed in the manifest; never read, never index-reused (the counter is monotonic and durable), just wasted disk. A startup sweep could delete files not in the manifest, if ever wanted.
**On-disk format compatibility is out of scope while building (decided 2026-10-30):** bumping `WAL_VERSION` or `SS_TABLE_VERSION` doesn't need a migration path or an old-format reader — wipe `data/` after a format change. Don't raise compatibility concerns on format changes until then. **Planned:** once the database is finalized, add versioning/compatibility so that from that point on, newer versions can still read files written by older ones (a format bump then needs a migration path or an old-format reader, not a wipe).
**Compaction ordering (crash safety, same principle as flush) — see "LSM Compaction" below for full detail:** write the merged SSTable → sync it + `data/` → **update the manifest (durably, `replace_ss_table_indices()`)** → *then* delete the old physical files. `compact()` never touches the WAL or `curr_mem_table` at all — those protect *not-yet-durable* writes, and compaction only ever operates on data that's already durable on disk; the two are independent durability domains that never need to interact.

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
- A Bloom filter's asymmetry (false positives allowed, false negatives forbidden) isn't an arbitrary convention — a **positive** answer always requires a real read anyway to get the value, so there's no work to save on that path regardless of which direction the asymmetry runs; the *only* place a filter can ever save a real read is on a **negative** answer, so whichever answer comes "for free" must be the trustworthy one ✅
- The Kirsch–Mitzenmacher double-hashing trick (`g_i = h1 + i·h2 mod m`, deriving k hash positions from just 2 real hash calls) is a real, standard technique — but it is *not* a perfect substitute for k truly independent hashes; measured against a from-scratch k-independent-hash baseline on identical keys, it produced a real, repeatable ~35% higher false-positive rate (0.136% vs. a 0.1% target) that persisted across many independent trials — a genuine, accepted engineering tradeoff (2 hash computations instead of k), not a bug ✅ (isolated via a controlled side-by-side comparison, not just theory)
- Sizing a Bloom filter (`m` bits, `k` hash functions) from a target false-positive rate `p` and element count `n` is a *different* problem from measuring the false-positive rate of an already-sized filter — the two formulas look similar but solve for different unknowns, and reusing the wrong one produces a filter that "looks right" but is silently mis-sized ✅
- `m` (a Bloom filter's bit-array size) is a *lower bound* — round it **up**, never to nearest and never down, when converting to a whole number of bits or bytes; rounding down measurably pushes the real false-positive rate above the target ✅ (found and fixed a real instance of this: applying the `(x+7)/8` integer ceiling-division trick directly to the *fractional* bit count before rounding it up to a whole number first, which silently under-allocates by up to a full byte in specific cases)
- Persisting a Bloom filter's bit-array length as **bytes** rather than **bits** was a deliberate mid-implementation design change from the original schema draft: a reader can then allocate and read the exact right number of bytes with no ceiling-division of its own, at the cost of needing to remember to multiply by 8 everywhere a bit-index is computed (the `k`-sizing formula and the hash-index math) — getting this backwards (using the byte count where a bit count was needed) was a real bug, hit twice in two different spots ✅
- A file format field's width (`uint32_t` vs `uint64_t`) must match *exactly* between writer and reader — this project hit the same "reader declares a narrower/wider local variable than what was actually written" bug **three separate times** across the v2 rewrite (a data block's `entry_cnt`, twice independently in two different functions, plus a sparse index entry's `key_len` in the opposite direction), each one silently misaligning every byte read afterward rather than failing cleanly at the point of the mistake ✅
- When verifying a checksum, the *stored* checksum value must never be folded into the value you're computing to compare against it — it's the answer key, not more input data. Hit and fixed **twice** in two independently-written new read functions (footer, Bloom filter block) despite the correct pattern already existing elsewhere in the same file (header, data block) ✅
- Unary minus on an unsigned type doesn't produce a negative number — it wraps to a huge positive value via modular arithmetic (`-uint32_t{20}` is `4294967276`, not `-20`); passing that into a signed API (like `seekg`'s offset parameter) doesn't "become negative" on the implicit widening conversion either, it stays a huge positive number pointing far outside the file. Same underlying class of surprise as unsigned loop-counter underflow, just showing up in offset arithmetic instead of a loop ✅ (found and fixed a real instance of this)
- `std::pair`'s default `operator<` is lexicographic — if you search a sorted `vector<pair<K,V>>` using `upper_bound` with a fabricated/dummy second component, an *exact match* on the first component falls through to comparing the fabricated second value, which can silently reorder the result relative to what searching by the first component alone would give. The fix is a custom comparator that only ever looks at the key, never the fabricated value — found via a very specific, easy-to-miss symptom: every key worked *except* ones that happened to exactly equal one of the sorted array's own stored keys (in this project's case, a sparse index's own block-starting keys) ✅
- `std::upper_bound`'s 4-argument (custom comparator) overload and its 3-argument (default `operator<`) overload are easy to conflate — passing `(first, last, comparator)` with the actual search value accidentally omitted doesn't call the comparator overload with a missing argument, it matches the *3-argument* overload instead, treating the comparator itself as the value to search for, and fails at compile time with a wall of "no viable `operator<`" candidate errors that don't obviously point back at the missing argument ✅
- A non-`void` function that falls off its closing brace without a `return` on every path is undefined behavior, not a guaranteed "returns some default/empty value" — confirmed concretely: the compiler flags it (`-Wreturn-type`), and at runtime it manifested as a `std::bad_alloc` crash from a `std::vector` member whose internal state was never actually initialized ✅
- **(Phase 7)** `steady_clock` vs `system_clock` — only `steady_clock` is guaranteed monotonic (can't jump backward when the system time is changed), so it's the only correct clock for measuring durations. `high_resolution_clock` is an implementation-defined alias (libc++/macOS: `steady_clock`; libstdc++/Linux: `system_clock`) — never use it for benchmarks ✅
- **(Phase 7)** Benchmarking vocabulary: throughput vs. latency; why percentiles (p50/p95/p99/p99.9/p99.99/max) beat an average — the average hides exactly the rare slow operations (flushes) users feel; which percentile a periodic stall lands in depends on its *frequency* (1 KB MemTable limit ≈ 1 flush per ~50 `SET`s ≈ 2% ⇒ visible at p99; 4 MB limit ⇒ invisible even at p99.99, and a 100k-insert run might never flush at all) ✅
- **(Phase 7)** The timer itself has a cost: back-to-back `steady_clock::now()` pairs measured ~14–18 ns average — that's the measurement floor; operations near it (MemTable hits) are the most distorted ✅
- **(Phase 7)** Relative file paths resolve against the process's **current working directory**, not the repo or the binary's location — running `./mewDb` from `build/` looks for `build/data/` (and now aborts at startup since `Wal`'s constructor throws when `fopen` can't create the file) ✅
- **(Phase 7)** `const`/`constexpr` at namespace scope gives **internal linkage** (each `.cpp` gets its own private copy) — removing `constexpr` from a header constant without adding `const` makes it a mutable global with external linkage defined in every TU ⇒ duplicate-symbol link error (ODR violation); `inline const` (C++17) gives external linkage with exactly one shared copy. Same root cause as the Phase 4 `inline const std::string` fix ✅ (verified by experiment)
- **(Phase 7)** The three places an acknowledged write can live: (1) the user-space stream buffer (lost on process crash), (2) the OS page cache (survives process crash, lost on power loss), (3) stable storage (only after `fsync`). On macOS there's a 4th distinction: `fsync` only moves data to the drive, `fcntl(F_FULLFSYNC)` also forces the drive's own volatile cache to stable media. `kill -9` only tests level 2 — it can't distinguish a flush from a sync; only power loss tests a sync ✅ (found via a real `kill -9` experiment that lost acknowledged `SET`s)
- **(Phase 7)** `std::fstream` doesn't expose its file descriptor, so it can't be `fsync`ed — `FILE*` + `fileno()` can ✅
- **(Phase 7)** `EINTR` — a blocking syscall (like a sync waiting on storage) interrupted by a signal returns `-1`/`EINTR` and should be retried; retrying is *only* safe for `EINTR`, not for real I/O errors ✅
- **(Phase 7)** fsyncgate (PostgreSQL, 2018): after a failed `fsync`, the kernel could mark the unwritten pages clean/drop them, so a *retried* `fsync` returned success with the data never on disk — hence the WAL's permanent `write_failed` latch: a later "apparent success" on an uncertain file must not be trusted ✅
- **(Phase 7)** C stdio rule: switching a `FILE*` from reading to writing requires an intervening positioning call (`fseek`/`fsetpos`/`rewind`) — the call is what matters, not where it seeks to ✅
- **(Phase 7)** Recovery policy has two independent axes: *what to replay* (point-in-time = stop at the first bad record so the recovered state is one that actually existed — skipping a record and replaying later ones produces a history that never happened) vs. *what to do with the bytes not replayed* (delete / quarantine / refuse to start). Per-record checksums mean later records *can* be verified — the reason to stop is consistency, not unverifiability ✅
- **(Phase 7)** A torn tail (expected after a crash) and mid-log corruption (bit rot / bug) are different events that must not be conflated — same lesson as Phase 4's "don't reuse one signal for normal termination and real corruption". When unsure which it is, choose the *safe* direction of error — a false "corrupt" alarm is harmless, a false "torn tail, expected" reassurance hides data loss (same asymmetry reasoning as the Bloom filter: pick which mistake is allowed based on what each mistake costs) ✅

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

## Bloom Filters + Sparse Index (SSTable v2) — completed 2026-09-29, closes out Phase 6's core

**The problem this solves:** compaction (Phase 5) bounds the *number* of SSTable files a read ever has to search, but does nothing about the *cost of searching each one that remains*. Two distinct costs: (1) a point lookup (`GET`/`EXISTS`/`DELETE`'s existence check) had to fully read and checksum-verify an entire SSTable file just to learn a key **isn't** in it — the single most common outcome, since a key lives in at most one file; (2) `RANGE`/`PREFIX_SCAN` materialized a whole file into memory to pull out a handful of matching entries. Two separate tools, not one: a **Bloom filter** (a cheap, in-memory "can I skip this file entirely?" check) attacks (1); a **sparse index** (jump near the right spot in a sorted file instead of scanning it linearly) attacks (2).

**Practice 6a (standalone Bloom filter) — design landed:** `n = 100`, target `p = 0.1%` → `m = -(n·ln p)/(ln 2)² ≈ 1437.76`, rounded **up** (never to nearest, never down — `m` is a lower bound on hitting the target rate) to `1438` bits → byte-aligned to `180` bytes (`1440` bits allocated) → `k = (m/n)·ln 2 ≈ 9.97` → `10`. Bit storage: `vector<uint8_t>` with manual bit-packing (not `vector<bool>`, whose packed/proxy-reference behavior is a known footgun). Hash derivation: xxHash (`XXH3_64bits_withSeed`) with two seeds (`6767`/`6969`), combined via the Kirsch–Mitzenmacher trick `g_i = h1 + i·h2 mod m` to derive all `k` bit positions from just 2 real hash calls.

**The measured false-positive-rate investigation** (the most substantial finding from 6a): across 5 independent trials (500,000 total probe queries), the measured false-positive rate landed consistently around **0.136%**, not the 0.1% designed for — and with that many samples, ~8 standard deviations from the target, this was real, not noise. Root-caused via a controlled comparison: same keys, same `m`/`k`/`n`, only the hash-derivation method varied — `h1 + i·h2` gave 0.136%, swapping in `k` fully independent hash calls on the *identical* keys gave 0.112% (within normal noise of 0.1%). Conclusion: the double-hashing trick is a real, accepted tradeoff (2 hash computations instead of `k`), not a bug — **deliberately kept**, since the absolute cost (an occasional extra wasted disk read) was judged not worth paying `k`× the hashing on every `SET`/`GET` forever.

**Sparse index practice — deliberately skipped**, not deferred. The plan was "standalone sparse index practice first, then integrate both together," but the practice design that came up (simulating blocks as an in-memory `vector<vector<pair<string,string>>>`, no real file) was recognized to remove the one thing worth practicing — real byte-offset tracking and `seekg`/`tellp` mechanics — before any code was written. Went straight to the real implementation instead.

**SSTable v2 format — full byte layout in `ss_table_schema.md`; the design principle that shaped it:** every section is either **fixed-size** (Header, Footer) or **self-terminating** (carries its own count, so a reader can parse and checksum-verify it without consulting anything outside itself) — Data Blocks (target `RECORDS_PER_BLOCK = 10` entries, last block may hold fewer), a Bloom Filter Block, and a Sparse Index Block (one `(first key, block offset)` entry per data block). The **Footer** is the only place any cross-reference lives at all — two offsets (`bloom_filter_offset`, `sparse_index_offset`), each independently checksummed — because every other boundary in the file is derivable from those two plus each block's own self-terminating structure.

**Bloom filter/sparse index sizing is computed dynamically, per file, at write time** — `n` is always the real entry count about to be written (`mem_table.size()`, or the merged map's size for `compact()`), never hardcoded. This was a real design correction: the practice's `n = 100` was only ever a test fixture; the real system's `flush()` is byte-size-triggered (deliberately, so entry count doesn't bound RAM if value sizes vary — an existing Phase 4 decision, unchanged), so real SSTables have a genuinely variable entry count, and a fixed-size Bloom filter would have been badly mis-sized for anything that wasn't exactly 100 entries.

**The Bloom filter block persists the bit array's length in *bytes*, not bits** — a deliberate deviation from the original schema draft (which specified bits, with a reader deriving byte length via `ceil(m/8)`). Storing bytes directly means a reader never redoes a ceiling-division that has to exactly match what the writer did — one less place for the two to silently drift apart. The cost: every place that needs *bits* (the `k`-sizing formula, and the `hash % ...` indexing at both write and read time) must remember to multiply the stored byte count by 8 first — getting this backwards was a real, repeated bug (see below).

**The read path is split into two independent shapes, matching the two problems above:**
- **Point lookup** (`get_value_from_ss_table`, used by `GET`/`EXISTS`): read the footer → check the Bloom filter (definitely-absent ⇒ skip this file) → `upper_bound` + step back on the sparse index to find the one candidate block (`begin()` result ⇒ key smaller than everything in this file ⇒ not present) → read that one block → linear-scan its ~10 entries. A tombstone found here **stops the search immediately** (same shadowing rule as always) rather than falling through to older files; "scanned the correctly-identified block, key not there" correctly concludes "not in this file at all" without checking anything else, since blocks are sorted sub-ranges of a sorted file.
- **Full scan** (`read_ss_table`, feeding `KEYS`/`RANGE`/`PREFIX_SCAN`/`compact()`): walk data blocks sequentially from right after the header, never touching the Bloom filter, sparse index, or footer at all — these commands need to potentially see most/all keys anyway, so the skip-optimizations don't help them. Stops via the header's own `entry_count` field. This was examined against this project's own "don't trust redundant/informational state as authoritative" lesson (`num_ss_tables`, `next_index`) and judged safe specifically *because* `entry_count` is sourced from the exact same variable that drives the block-split loop in the same write call (no independent-drift path the way those earlier bugs had), and is now itself checksum-protected as part of the header — a corrupted count would be caught there, before the block loop ever starts.

### Bugs found and fixed during the v2 rewrite

An unusually long list — this was a full file-format rewrite, not one algorithm — worth keeping as a pattern library, same spirit as the Phase 4/5 lists above. Grouped by where they were found:

**Practice 6a:** missing `#include <algorithm>`/`<cctype>` (worked only via transitive includes); missing newline on `GET`'s REPL output (broke scripted testing, since consecutive results glued together with no delimiter); a dead duplicate `bloomFilter_idx` variable; an unused `mpp` map left over from copying the REPL pattern.

**Write path (`write_to_ss_table` and its helpers):**
1. `write_data_block` computed `entry_cnt` and folded it into the checksum, but never actually wrote it to the file — broke the self-terminating design outright.
2. `BLOOM_FILTER_TARGET_FP_RATE` declared `constexpr uint32_t` and assigned `0.001` — silently truncated to `0`.
3. `hash_func_cnt` (`k`) computed from the byte count instead of the bit count — off by a factor of 8 (gave `k=1` instead of `k=10` for the exact case already validated in practice).
4. The byte-ceiling-division was applied directly to the raw *fractional* bit count instead of a properly rounded-up whole number first — could silently under-allocate by up to a full byte (concrete counterexample: a fractional count of `1440.1` needs `181` bytes; the buggy formula gave `180`).
5. `write_bloom_filter_block` took the bit-array `vector` by value instead of `const&` — an unnecessary full copy on every flush/compact.
6. `ln2` and `BYTE_SIZE` were mutable, non-`const`, externally-linked file-scope globals — fixed to `const` (`BYTE_SIZE` also relocated to `constants.h`, next to its siblings).
7. The sparse index never got an entry for the file's **trailing partial data block** — confirmed via direct byte-level inspection of a real written file (3 real data blocks, only 2 sparse index entries); any key living only in that last block was unreachable by point lookup.
8. `write_sparse_index_block` — the exact same "computed but never written" `entry_cnt` bug as #1, independently reintroduced in a new function.
9. A dead `header_offset` local (computed via `tellp()`, never used) — removed.
10. **Known, accepted, open gap:** `total_entry_cnt == 0` divides by zero in the `k`-sizing formula (well-defined for `double`, but the subsequent `uint32_t` cast of the resulting `inf`/`nan` is UB) — deliberately deferred rather than fixed, on the judgment that it's not yet known to be reachable.

**Read path (`read_ss_table`, rewritten for blocks):**
11. The per-block `entry_cnt` was read into a `uint32_t` local where the actual on-disk field is `uint64_t` (8 bytes written, 4 read) — misaligned every byte read afterward.
12. `key_val_checksum` was called **twice** per entry — guaranteed checksum mismatch on every read, since the writer only folds each entry in once.
13. `while(entry_cnt--)` — decrementing an unsigned `0` wraps to `UINT64_MAX`; harmless *only* because nothing read the variable again afterward, replaced on principle with an explicit-index `for` loop.
14. A defensive `block_entry_cnt > total_entries` guard was **added proactively** (not requested) — self-recognized as preventing the exact "near-infinite loop/hang from a corrupted count" bug class already hit once in the v1 reader (see Phase 4's bug list) from recurring in the new block-based one.

**Point-lookup path (`get_value_from_ss_table`, footer/Bloom-filter/sparse-index reads):**
15. Both `read_footer_block` and the Bloom filter block reader folded the just-read *stored* checksum value into the running CRC before comparing against it — guaranteed mismatch, even on a genuinely valid file. The correct pattern (don't fold the stored value in) already existed elsewhere in the same file (header, data block reads) but wasn't applied to these two new functions at first.
16. `ss_table_file.seekg(-FOOTER_SIZE, std::ios::end)` — `FOOTER_SIZE` is `uint32_t`; unary minus on an unsigned type wraps to a huge positive value (`4294967276`, not `-20`) rather than becoming negative, so this tried to seek billions of bytes past the end of the file. Fixed with an explicit signed cast before negating.
17. `DataBlockData::entry_cnt` declared `uint32_t` where the real field is `uint64_t` — same class of bug as #11, in a new struct.
18. `read_sparse_index_block`'s local `key_len` declared `uint64_t` where the real field (matching every other `key_len` in the format) is `uint32_t` — the opposite-direction version of the same width-mismatch bug.
19. `read_data_block_by_offset` was missing its `return data;` entirely — fell off the end of a non-`void` function (undefined behavior). Confirmed two ways: the compiler's own `-Wreturn-type` warning, and a runtime `std::bad_alloc` crash from touching the never-initialized `std::vector` member.
20. A redundant duplicate call to `read_footer_block` (read the whole footer twice just to get its two different fields separately) — harmless but wasteful, consolidated to one call.
21. **The most subtle bug of the rewrite:** the sparse-index point lookup searched with `std::upper_bound(..., std::make_pair(search_key, 0))`. `std::pair`'s default comparison is lexicographic — if the first elements are equal, it falls back to comparing the second. Since the fabricated second value (`0`) is always smaller than any real stored byte offset, an *exact* key match against a sparse index's own stored key made the search pair compare as "less than" the real entry, throwing off `upper_bound`'s result specifically and only for keys that exactly equal one of the sparse index's own indexed keys (i.e., every block's first key). Surfaced by the extensive test pass below (`key00` — a real, tombstoned key — incorrectly reported `not_found`). Fixed with a custom comparator that only ever compares the key string, never a fabricated value. A follow-up mechanical slip while integrating the fix (the actual search-value argument was accidentally dropped, leaving only 3 arguments where `upper_bound`'s comparator overload needs 4) was caught immediately at compile time.

### Exhaustive test pass — completed 2026-09-29, closes out Phase 6's core

Ran a full adversarial pass against the real REPL binary (not synthetic unit probes) covering the whole command surface against the new format:
1. 300 keys sized to force ~7 flushes (7 SSTables, multiple 10-record blocks each) — all `SET`s and immediate `GET`s back matched exactly.
2. Deleted every 7th key (43 tombstones) and overwrote every 11th non-deleted key with a new value, then forced another flush so both landed on disk rather than staying in memory — `GET`/`EXISTS`/`KEYS`/`RANGE`/`PREFIX_SCAN` all correct across all 400 live keys (deleted ⇒ `NOT_FOUND`/`FALSE`, overwritten ⇒ newest value, `KEYS` membership exact).
3. `COMPACT`, then re-ran the *entire* check from (2) again against the single merged file — identical results. This incidentally exercised all 40 block boundaries in the compacted file (400 keys ÷ 10/block), since every individual key passed.
4. Crash/restart persistence across **two separate process runs** on the same `data/`: wrote 2 new keys small enough to stay WAL-only (unflushed), re-deleted an already-tombstoned key (correctly `NOT_FOUND`, confirming the exists-before-delete check still works against v2), exited, then started a **fresh process** — WAL replay correctly recovered both new keys, the old tombstone was still gone, and the final `KEYS` count matched the expected total (359) exactly.

Zero errors, zero crashes, across roughly 2,000 commands. `compact()` needed **no changes at all** for the v2 format — it only ever calls `SsTable`'s public methods, same as before.

---

## Phase 7 — Benchmark Tool (in progress, started 2026-09-29)

**Why this phase exists:** every test through Phase 6 checked *correctness*; nothing measured *cost*. Phase 6's whole motivation (Bloom filters skip files, sparse index reads one block) is a performance claim that has never been measured. The benchmark turns claims into numbers, unblocks the deferred tiered-compaction design (to be designed against real numbers, not guesses), and gives Phase 8 (TCP + concurrency) a baseline to compare against. A benchmark is itself a measuring instrument and can be buggy (timing the wrong thing, dead-code elimination, page-cache-served "disk" reads) — trusting it needs the same discipline as trusting a checksum.

### Benchmark design decisions made so far (no benchmark code written yet)
- **In-process, linked directly against `DbEngine`** (calling `.set()`/`.get()`), not driving the REPL via subprocess + pipe. Reason: REPL/pipe/parse/format overhead is a fixed per-op cost comparable to or larger than a MemTable `GET` — it doesn't just add time, it compresses relative improvements and adds its own variance. **End-to-end benchmark deferred to Phase 8**, where it'll go over TCP (the REPL is about to be retired as the real front-end, so benchmarking through it now would measure something being thrown away). Comparing "over TCP" vs. "in-process" in Phase 8 then isolates what the network/server layer costs.
- **Clock:** `std::chrono::steady_clock`, nanosecond resolution.
- **Timed region:** only the `DbEngine` call itself (plus the two unavoidable `now()` calls). Excluded: key/value generation, choosing which operation to run, recording latencies, sorting/percentile computation. **All keys pre-generated before the run, from a fixed-seed RNG** (`std::mt19937_64`) so every run is reproducible. `reserve()` the latency vector up front.
- **Two separate passes over the same keys:** a *latency pass* (per-op timers → percentiles) and a *throughput pass* (one timer around the whole loop, no per-op timers → ops/sec without timer overhead).
- **Report:** p50, p95, p99, p99.9, p99.99, max per operation type; **flush count**; **SSTable count at end of run** (read cost scales with it — see table-cache finding below).
- **MemTable limit is a swept variable, not a single choice:** fix the total data volume written (e.g. ~100 MB) and sweep the limit (e.g. 64 KB / 1 MB / 4 MB) so only one thing changes between runs. Real-world reference points: LevelDB `write_buffer_size` 4 MB, RocksDB 64 MB. The current 1000-byte limit was a deliberate dev-testing choice from Phase 4, never meant to be realistic. Bigger limit ⇒ fewer flushes/SSTables (lower read amplification, esp. with no auto-compaction) but more RAM, longer WAL replay, and a bigger (rarer) stall per flush.
- **Benchmark gets its own data directory and wipes it itself** (`std::filesystem::remove_all`) before constructing `DbEngine` — explicitly *not* a "clear DB" command in the product (that would put a destroy-everything command into the public API, reachable by any client in Phase 8, purely for the benchmark's convenience). Needs the `Options` struct below. **Reminder when implementing:** guard `remove_all` against an empty path, `"."`, and `"data"`.
- **Noise control:** warm-up runs remove first-run effects (page cache, CPU caches, allocator growth, CPU frequency ramp); multiple trials *measure* scheduler/background variance rather than removing it; neither fixes thermal throttling/Spotlight/other apps — control the environment (plugged in, apps closed) and report the median across trials plus the spread. Benchmark must be a Release (`-O2`) build.
- **Expected `GET` cost ranking** (cheapest → most expensive): MemTable hit → newest SSTable (or a tombstone found there) → missing entirely (a Bloom check per SSTable, occasional false-positive block read) → oldest SSTable (a Bloom check per SSTable + sparse index + one block read). "Missing" is cheaper than "oldest" *because of* Phase 6's Bloom filters.

### Finding: Bloom filters are read from disk on every lookup (table cache — measure first, then fix)
`DbEngine::get()` constructs a fresh `SsTable` per SSTable per lookup (`db_engine.cpp`'s `get()` loop), and `get_value_from_ss_table()` re-opens the file, reads + checksums the footer, then reads + checksums the Bloom filter block from disk every time. The Bloom filter is therefore *not* the cheap in-memory skip-check the Phase 6 design assumed — a missing-key `GET` against ~2,000 SSTables (the 1 KB limit after 100k inserts, no auto-compaction) means ~2,000 file opens and ~4,000 checksummed reads. **Deliberately not fixed yet:** the benchmark should confirm and quantify it first (predict how missing-key `GET` latency scales with SSTable count, then measure). The eventual fix is a "table cache" — keep per-SSTable footer/Bloom filter/sparse index in memory across lookups; open question for then: RAM cost per SSTable.

### Planned: `Options` struct (designed, not yet implemented)
| Field | Type | Default | Why |
|---|---|---|---|
| `data_dir` | `std::filesystem::path` | `"data"` | one directory; every filename derived from it |
| `mem_table_size_limit` | `uint64_t` | `MEM_TABLE_SIZE_LIMIT` | benchmark sweeps it; REPL keeps 1000 |
| `wal_sync` | `enum class` | flush level | durability level — lowest offered level is *flush* (never "buffered only" — that's the proven bug); also makes the currently hard-coded `F_FULLFSYNC` configurable |
- **One directory, not four separate file paths** (WAL / manifest / temp manifest / SSTable prefix): fewer invalid configurations, and the temp manifest *must* be on the same filesystem as the manifest or the atomic `rename()` fails.
- REPL keeps working unchanged: struct default member initializers = the existing constants + a defaulted `DbEngine` constructor argument, so `DbEngine Db;` still compiles.
- `DbEngine` stores the `Options`, passes `data_dir` to the `Wal`/`Manifest` constructors, and calls `std::filesystem::create_directories(data_dir)` **before** constructing them (both create files in their constructors).
- **Exactly one function owns "index → SSTable path"** — today it's built in two places (`db_engine.cpp` in `compact()`'s file deletion, and `SsTable`'s constructor in `ss_table.cpp`) — the "don't track the same fact in two places" lesson again. Put it in `ss_table.h`.
- **Trap to fix while threading the path through:** `manifest.cpp` already mixes the `manifest_file_name` member and the raw `MANIFEST_FILE_NAME` constant for the same file (`replace_ss_table_indices()` renames/reopens via the constant) — change only one and renames go to one path while reads come from another. 9 call sites of the filename/limit constants across 4 files.
- Deliberately *not* in `Options` yet: `RECORDS_PER_BLOCK`, `BLOOM_FILTER_TARGET_FP_RATE` — both are safe to make configurable later (readers use each block's stored `entry_cnt` and the stored Bloom parameters, never the constants), if the benchmark wants to sweep them.

### WAL durability detour (2026-10-03 → in progress)
**How it started:** researching `fsync` for the benchmark raised "when `SET` returns `OK`, where does the WAL record actually live?" `Wal::write()` made nine `fstream::write()` calls and never flushed; the only `close()` calls were in `truncate()` and the destructor. A `kill -9` experiment (SET 3 keys → kill → restart → GET) **lost all 3 acknowledged keys** — they were still in the stream's user-space buffer. The Phase 6 "two-process WAL-recovery test" had passed only because that first process exited via `EXIT`, running the destructor which flushed the buffer — it proved recovery after a *clean shutdown*, never after a crash.

**What landed (commit `e24b1fd`, "FIX: Durable WAL writes with fsync and torn-tail recovery"):**
- `Wal` switched from `std::fstream` to `FILE*` (`unique_ptr` with `fclose` deleter) so the file descriptor is reachable for syncing.
- Every record: `fwrite` the fields → `fflush` → `fcntl(fd, F_FULLFSYNC)` (macOS), retried on `EINTR` only. **Hard-coded strongest level on every `SET`** — makes every REPL test/benchmark pay the full sync cost until `Options.wal_sync` exists. `kill -9` experiment re-run: keys now survive (note: that proves the flush, not the sync — only power loss tests the sync; its cost is the observable evidence it's happening).
- `write_failed` latch — after any write/sync failure, all later writes fail until restart (fsyncgate reasoning, see "Concepts understood").
- `set()`/`del()` throw on WAL failure instead of silently dropping the write.
- **Off-by-one fixed:** after recovery `index` was set to the last record's index (so the next write reused it); now `record.index + 1`.
- `read_string()` bounds a stored length against the bytes actually left in the file — a corrupt 4 GB `key_len` used to mean a 4 GB allocation.
- **Torn-tail bug fixed.** Before: recovery stopped at a torn trailing record but left it in the file, and `"a+b"` appended new records *after* the garbage — so every later restart stopped at the same garbage and never reached them. Acknowledged, synced writes were silently lost (reproduced: `SET b` → `OK` → restart → `NOT_FOUND`). Now `recover()` remembers `last_valid_end` (offset after the last record whose checksum passed), `ftruncate`s back to it, and syncs before any new append.

**Recovery policy decided (2026-10-30): point-in-time recovery + truncate ("option A").** Replay stops at the first invalid record (later records are *not* replayed even if valid — that would produce a state that never existed); everything from that point is truncated so new appends stay reachable. **Accepted cost, chosen knowingly:** a corrupt record in the *middle* of the log permanently discards every acknowledged write after it (reproduced: one flipped bit in record 1 shrank a 92-byte WAL to 30, destroying a valid record 2). Rejected alternatives: B (quarantine discarded bytes to a side file, then truncate), C (refuse to start if complete records follow the bad one — close to RocksDB's `kTolerateCorruptedTailRecords`). For reference, RocksDB's `WALRecoveryMode` has four modes; mewDb's replay policy is `kPointInTimeRecovery`.

**Discard reporting:** `recover()` classifies why it stopped (`StopReason::torn_tail` vs `corrupt_record` — a short read ⇒ torn tail; a fully-read field with a bad value (magic/version/operation) or a failed header/record checksum ⇒ corrupt, since a torn append always leaves a *valid prefix* of a record) and prints to stderr: `WAL recovery: discarded <N> bytes at offset <M>: torn tail (expected after a crash)` or `...: corrupt record (possible data loss)`. Silent on a clean log. (Previously only a checksum mismatch printed `Recovery stopped`; every other stop and the truncation itself were silent.)

**Fixed (2026-10-30) — corrupted length fields were labelled "torn tail".** A corrupted *length* field (one whose claimed length runs past EOF) used to be classified as `torn tail (expected after a crash)` — reproduced: record 1's `key_len` corrupted to 5000 with valid records 2 and 3 after it ⇒ all discarded, labelled "expected". That's the dangerous direction of error (false reassurance hiding real data loss). Options discussed: (a) a third, honest "can't tell" label; (b) WAL v2 with a separate header checksum over version/index/operation/lengths so a length can be trusted before using it; (c) on a stop, scan the discarded bytes for a complete record with a matching checksum — if one exists it's definitely corruption. **Chose (b)**: (a) and (c) both accept the ambiguity and pick a direction to be wrong in, while (b) removes it; (c) also can't decide a corrupted length on the *final* record (nothing valid after it to find). With (b), every stop has a definite label — file ends inside the header ⇒ torn; header complete but `header_crc` fails ⇒ corrupt; header passes but the file ends inside key/value/`record_crc` ⇒ torn (lengths are trusted now); `record_crc` fails ⇒ corrupt. (c) is deliberately **not** added yet — with (b) it's no longer needed for classification, only as an optional extra ("N valid records were discarded") in the corruption message. Tests: `test_mid_log_length_corruption_is_reported_as_corrupt` (the exact repro) plus header-checksum/key-byte/length mutations in `test_invalid_record_fields_and_checksum`.

### Phase 7 open items / reminders (don't let these disappear)
1. ~~**Fix the length-field misclassification**~~ — done 2026-10-30 via WAL v2 header checksum (see above).
2. ~~**Commit**~~ — done (`1e03c94` discard reporting, `304b361` WAL v2).
2a. ~~**Decide: what happens to a v1 WAL on upgrade?**~~ — decided 2026-10-30: **ignore backward compatibility while the database is being built** (see Key design decisions). A v1 record fails the version check and is discarded as corrupt, like any unknown version (covered by the `version` mutation in `test_invalid_record_fields_and_checksum`). Wipe `data/` after a format bump.
3. ~~**Sync the flush path**~~ — done 2026-10-31 (`a054af1`), see "Durability gap — closed" under Key design decisions.
3a. **Found 2026-10-31 — `DELETE` never triggers a flush.** `del()` grows `mem_table_size` by `key.length()` but never checks `MEM_TABLE_SIZE_LIMIT` (only `set()` does), so a delete-only workload grows the MemTable and the WAL without bound. **Linked to known gap #10** (zero-entry SSTable divides by zero in the Bloom `k` formula): today an all-tombstone `COMPACT` is unreachable *only because* flushes happen solely on `SET`, so the newest SSTable always holds at least one live value. Verified on a scratch copy with a flush check added to `del()`: 100 `SET`s → 100 `DELETE`s → `COMPACT` ⇒ a 0-entry SSTable ⇒ the next `GET` segfaults (exit −11). **Fix both together.**
3b. **Found 2026-10-31 — `DELETE` is the most expensive command.** To decide `OK` vs `NOT_FOUND`, `del()` copies the entire MemTable and calls `get_keys_from_ss_table()` on *every* SSTable, which does a full `read_ss_table()` of each file — O(all data on disk) per `DELETE`, ignoring the Bloom filter and sparse index entirely. The point-lookup path already exists (`exists()`/`get()`). Benchmark should show this; fixing it is straightforward.
3c. **Benchmark note:** a flush now does 5 `F_FULLFSYNC`s (SSTable, `data/`, temp manifest, `data/` again, WAL truncate). On macOS `F_FULLFSYNC` flushes the whole drive cache, so back-to-back calls each pay roughly full price; `F_BARRIERFSYNC` (ordering-only, cheaper) is worth reading about once the benchmark shows what flushes cost.
4. **Implement `Options`** (design above).
5. **Practice:** `practice/chrono.cpp` at `-O0` vs `-O2` (expect the empty loop to be eliminated at `-O2` — dead-code elimination), and `microseconds` → `nanoseconds`.
6. **Q11 predictions (user to write down before any measurement is shared):** `SET` latency at each durability level (buffered / flush / `fsync` / `F_FULLFSYNC`); max single-threaded `SET`s/sec if one `fsync` costs 1 ms; why that arithmetic is the reason "group commit" exists. A real per-`SET` measurement with `F_FULLFSYNC` was taken during the 2026-10-30 test pass — **don't reveal it until the predictions are written** (re-measure if needed).
7. **Research still to discuss in the benchmark design:** LSM write/read/space amplification, `db_bench` (LevelDB/RocksDB) and YCSB, workload shape (sequential vs. random keys, uniform vs. Zipfian/hot-key), `<random>` + fixed seeds, warm-up + multiple trials.
8. **Then design and build the benchmark.**
9. **Table cache** — only after the benchmark measures the Bloom-filter-from-disk problem.
10. **Persistent WAL sequence number (needed for Phase 9):** `index` resets to 0 on a restart that follows a flush (the flush empties the WAL, so recovery has nothing to read the last index from). Needs to be stored durably elsewhere (likely the manifest). Second use, verified 2026-11-02 with lldb on `a054af1`: a kill between the manifest rename and `Wal::truncate()` leaves a complete SSTable *and* the untruncated WAL, so the restart replays records that are already in that SSTable. Harmless today only because `SET`/`DELETE` are idempotent (applying a record twice = applying it once): every `GET` was correct, the next flush simply wrote the same 72 keys into a second SSTable, and `COMPACT` removed the duplicate. A non-idempotent operation (e.g. an `INCR`) would be applied twice. Storing "last WAL sequence number flushed" in the manifest would let replay skip those records instead of relying on idempotence (this is what RocksDB's manifest log number does).
11. **End-to-end benchmark over TCP** — Phase 8.
12. **Untested Windows code** in `wal.cpp` (`_commit`, `_fseeki64`, `_ftelli64`, `_chsize_s`) — never compiled; user's call whether to keep it.
13. **Still deferred (unchanged, don't re-litigate):** automatic compaction triggering (synergy with Phase 8 background threads) and tiered/size-based compaction (design against Phase 7 numbers).

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
├── ss_table_schema.md     ← authoritative SSTable v2 byte-level format (Header/Data Blocks/
│                            Bloom Filter Block/Sparse Index Block/Footer)
├── CMakeLists.txt         ← complete and building cleanly; links zlib + xxHash
├── main.cpp               ← full REPL, all commands wired incl. COMPACT, DbEngine Db; (no-arg ctor), trim() for EXIT/QUIT
├── calc.cpp               ← practice REPL (complete, not part of main build)
├── include/
│   ├── file_io.h          ← shared durability helpers: write_bytes, file_position, sync_file, sync_parent_directory
│   ├── constants.h        ← all shared format constants + OpenMode enum + v2 constants (RECORDS_PER_BLOCK,
│   │                        BYTE_SIZE, FOOTER_SIZE, BLOOM_FILTER_TARGET_FP_RATE, BLOOM_FILTER_SEED_1/2)
│   ├── db_engine.h        ← DbEngine class (owns Wal*, Manifest*; compact()) — unchanged by the v2 rewrite
│   ├── wal.h              ← Wal class (write, recover, truncate; no-arg ctor; FILE* + write_failed latch since Phase 7)
│   ├── ss_table.h         ← SsTable class — same public surface as before, v2 internals (see ss_table_schema.md)
│   └── manifest.h         ← Manifest class (durable SSTable index registry; replace_ss_table_indices) — unchanged
├── src/
│   ├── db_engine.cpp      ← DbEngine implementation (curr_mem_table, tombstone-aware, + WAL + SSTable + manifest + compact)
│   ├── wal.cpp            ← WAL implementation (per-record fflush + F_FULLFSYNC, point-in-time recovery + tail repair,
│   │                        discard reporting on stderr, truncate, checksum)
│   ├── ss_table.cpp       ← SSTable v2 implementation — block/footer/Bloom-filter/sparse-index write+read helpers,
│   │                        point-lookup path (get_value_from_ss_table) and full-scan path (read_ss_table)
│   ├── manifest.cpp       ← manifest implementation (load-or-create, write_manifest: temp → sync → rename → sync dir)
│   └── file_io.cpp        ← durability helpers (F_FULLFSYNC/fsync with EINTR retry, directory sync)
├── practice/
│   ├── encoder.cpp        ← binary encoder/decoder practice (complete)
│   ├── wal.cpp            ← standalone WAL write + recover practice (complete)
│   ├── bloom_filter.cpp   ← standalone Bloom filter practice (6a, complete)
│   └── chrono.cpp         ← Phase 7 std::chrono timing practice
├── tests/                 ← GITIGNORED — Claude-maintained test suite, not part of the repo
│   ├── wal_recovery_test.py   ← WAL crash-recovery tests (run: python3 tests/wal_recovery_test.py <mewDb binary>)
│   ├── command_surface_test.py ← all commands across flushes/restart/COMPACT (same invocation)
│   └── crash_stress_test.py   ← randomized SIGKILL crash-consistency test (MEWDB_CRASH_TRIALS/SEED env vars)
├── data/                  ← runtime output: wal.bin, ss_table_N.bin (v2 format), manifest.txt — gitignored wholesale
└── build/                 ← generated by cmake (gitignored)
```
