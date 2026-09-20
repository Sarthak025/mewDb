# SSTable Format v2 — Schema (Phase 6: Bloom filter + sparse index)

Status: **Finalized.**

This supersedes the v1 format documented in `CLAUDE.md` (single header + flat entry list +
one whole-file checksum). It is a breaking change — `SS_TABLE_VERSION` bumps from `1` to `2`,
and v1 files will not be readable by v2 code. This project has no production data to migrate,
so the assumption below is that `data/` gets wiped and everything is regenerated from a fresh
run. **Flag if that's wrong.**

Design principle carried through every section below: every section is either **fixed-size**
(Header, Footer) or **self-terminating** — it carries its own count, so a reader can parse it
completely and verify its checksum without consulting anything outside itself. The only
cross-references needed anywhere are the two offsets stored in the Footer.

---

## Global constants (proposed names, `constants.h`)

| Constant | Value | Meaning |
|---|---|---|
| `SS_TABLE_VERSION` | `2` | bumped from `1` |
| `SS_TABLE_MAGIC_NUMBER` | `0xDEADBEEF` (unchanged) | file-type identifier |
| `RECORDS_PER_BLOCK` | `10` | target entries per data block (last block may hold fewer) |
| `BLOOM_FILTER_TARGET_FP_RATE` | `0.001` (0.1%) | used only at write time to derive `m`/`k`; never stored — the file only stores the results |
| `BLOOM_FILTER_SEED_1` / `BLOOM_FILTER_SEED_2` | `6767` / `6969` | xxHash seeds for `h1`/`h2`; global, not per-file |

All confirmed: `RECORDS_PER_BLOCK = 10`, `BLOOM_FILTER_TARGET_FP_RATE = 0.001`, seeds
`6767`/`6969`.

---

## File layout

```
[ Header ]                 <- fixed size, offset 0
[ Data Block 0 ]
[ Data Block 1 ]
...
[ Data Block B-1 ]
[ Bloom Filter Block ]      <- starts at footer.bloom_filter_offset
[ Sparse Index Block ]      <- starts at footer.sparse_index_offset
[ Footer ]                  <- fixed size, last thing in the file
```

Every boundary is derivable, nothing else needs to be stored:
- Data blocks start right after the (fixed-size) header and are walked sequentially — each
  block's own `entry_cnt` tells a reader exactly where that block ends and the next begins.
  A sequential scan (`KEYS`/`RANGE`/`PREFIX_SCAN`) stops walking data blocks the moment its
  read position reaches `bloom_filter_offset`.
- The Bloom Filter Block and Sparse Index Block are each self-terminating (see below), so
  their own end doesn't need to be stored either — only their *start* offsets, which live in
  the Footer.
- The Footer is always the last `FOOTER_SIZE` bytes of the file — found by seeking to
  `file_size - FOOTER_SIZE`, no scanning required.

---

## Header (fixed size, offset 0)

| Field | Type | Size | Notes |
|---|---|---|---|
| `magic_number` | `uint32_t` | 4 | `SS_TABLE_MAGIC_NUMBER` |
| `version` | `uint8_t` | 1 | `SS_TABLE_VERSION` (= 2) |
| `ss_table_index` | `uint64_t` | 8 | must match the filename, same as v1 |
| `entry_count` | `uint64_t` | 8 | total entries in the file — `curr_mem_table.size()` at flush time (the merged map's `size()` for `compact()`); informational only, same spirit as the manifest's `num_ss_tables` — nothing re-derives it from the blocks, it's just written once, up front, since the count is already known before any block is written |
| `header_checksum` | `uint32_t` | 4 | CRC32 over `version` + `ss_table_index` + `entry_count` — `magic_number` is deliberately excluded, matching how the v1 whole-file checksum already excludes it (magic number is the "is this even the right file type" check, done before you decide to trust anything else) |

Total: **25 bytes**, fixed.

---

## Data Block (variable size, repeated)

| Field | Type | Size | Notes |
|---|---|---|---|
| `entry_cnt` | `uint64_t` | 8 | number of entries in *this* block (≤ `RECORDS_PER_BLOCK`; the last block in the file may hold fewer) |
| entries × `entry_cnt` | — | variable | same per-entry shape as v1: `operation`(`uint8_t`) + `key_len`(`uint32_t`) + `key` + `val_len`(`uint32_t`) + `val` |
| `block_checksum` | `uint32_t` | 4 | CRC32 over `entry_cnt` + every entry in this block |

Self-terminating: read `entry_cnt`, loop that many times parsing length-prefixed entries,
then the next 4 bytes are the checksum — exactly the same technique v1 already uses for the
whole file, just scoped to one block.

---

## Bloom Filter Block (variable size, starts at `footer.bloom_filter_offset`)

| Field | Type | Size | Notes |
|---|---|---|---|
| `bit_array_size` | `uint64_t` | 8 | **bytes**, not bits — computed at write time as `ceil(m/8)` where `m` comes from this file's real entry count and `BLOOM_FILTER_TARGET_FP_RATE` (never hardcoded); a reader uses this value directly as "how many bytes to read," no ceiling math needed on read |
| `k` | `uint32_t` | 4 | hash-function count, computed from the *bit* count (`bit_array_size * 8`), not the byte count — easy to get backwards, see the implementation note below |
| `bit_array` | `uint8_t[]` | `bit_array_size` | length is read directly, not derived |
| `checksum` | `uint32_t` | 4 | CRC32 over `bit_array_size` + `k` + `bit_array` |

`bit_array_size` and `k` **must** be stored (not derived at read time) — deriving them would
require reconstructing this file's true entry count, which means reading every data block,
which defeats the entire point of having a cheap skip-check in the first place.

Storing the **byte** count rather than the raw bit count `m` was a deliberate choice made
during implementation, not the original plan (which stored bits and expected a reader to
`ceil()` its way to a byte length): reading `bit_array_size` bytes directly means a reader
never redoes a ceiling-division that has to exactly match what the writer did — one less
place for writer/reader logic to quietly drift apart. The tradeoff: **every place that needs
bits — the `k` formula at write time, and the `hash % ...` indexing at both write and read
time — must remember to multiply by 8 first.** Getting this backwards (using the byte count
where a bit count is needed) was a real bug hit during implementation, twice, in two
different spots — worth double-checking both spots stay consistent as the sparse index and
`db_engine` integration get built next.

The `h1 + i·h2 mod m` derivation and its known, measured ~0.136%-vs-0.1% false-positive gap
(from the practice run) carries forward unchanged — already a deliberate, accepted tradeoff.

---

## Sparse Index Block (variable size, starts at `footer.sparse_index_offset`)

| Field | Type | Size | Notes |
|---|---|---|---|
| `entry_cnt` | `uint64_t` | 8 | number of sparse entries = number of data blocks |
| entries × `entry_cnt` | — | variable | `key_len`(`uint32_t`) + `key` (the **first** key of the corresponding data block) + `block_offset`(`uint64_t`) |
| `checksum` | `uint32_t` | 4 | CRC32 over `entry_cnt` + every entry |

Self-terminating the same way as a data block. In memory this becomes a sorted
`vector<pair<string, uint64_t>>`; lookup is `upper_bound(target)` then step back one
(`begin()` result ⇒ key smaller than everything in this file ⇒ not present, no decrement).

---

## Footer (fixed size, last thing in the file)

| Field | Type | Size | Notes |
|---|---|---|---|
| `bloom_filter_offset` | `uint64_t` | 8 | start of the Bloom Filter Block |
| `sparse_index_offset` | `uint64_t` | 8 | start of the Sparse Index Block |
| `footer_checksum` | `uint32_t` | 4 | CRC32 over the two offsets above |

Total: **20 bytes**, fixed. Found by seeking to `file_size - 20`.

---

## How each command uses this

**`GET`/`EXISTS`/`DELETE`'s existence check (point lookup), per SSTable, newest-to-oldest:**
1. Seek to `file_size - FOOTER_SIZE`, read the footer, verify its checksum.
2. Read the Bloom Filter Block at `bloom_filter_offset`, verify checksum, query membership.
   - Definitely absent → skip this file entirely, move to the next-older SSTable.
3. Maybe present → read the Sparse Index Block at `sparse_index_offset`, verify checksum,
   `upper_bound` + step back to find the right data block (or conclude "not present" on
   `begin()`).
4. Read that one data block, verify its checksum, linear-scan its entries for the key.
   - Found, tombstone → **stop the whole search now**, report not-present (do not check
     older files — same shadowing rule as today).
   - Found, has value → return it.
   - Not found in this block → the key is not in this file at all (blocks are sorted
     sub-ranges of a sorted file, so if it existed here it would have to be in this exact
     block) → move to the next-older SSTable.

**`KEYS`/`RANGE`/`PREFIX_SCAN`:** walk data blocks sequentially from right after the header,
each block self-terminating, until the read position reaches `bloom_filter_offset`. Neither
the Bloom filter nor the sparse index is consulted — these commands need to potentially see
most/all keys anyway. *(Possible future optimization, not needed now: `RANGE` could use the
sparse index to jump straight to the block containing its start key instead of scanning from
block 0 — worth revisiting later, not required for this pass.)*

---

## Decisions log

All open questions from the draft are resolved:
1. `data/` gets wiped and regenerated from scratch — no v1→v2 migration.
2. Header keeps a global `entry_count` (informational, sourced from `curr_mem_table.size()`
   or the compacted map's `size()` — never re-derived from the blocks).
3. `RECORDS_PER_BLOCK = 10`, `BLOOM_FILTER_TARGET_FP_RATE = 0.001`, seeds `6767`/`6969`.
4. `k` stays `uint32_t`; every other count/offset/index field stays `uint64_t` — deliberately
   kept as a mix rather than flattened to one width. The split follows one rule: fields that
   are inherently bounded by what they mean (`key_len`, `val_len`, `k`) stay 32-bit; fields
   that either repeat only once per file/block (so widening them costs nothing meaningful) or
   grow for the entire lifetime of the system (offsets, entry counts, `ss_table_index`, and —
   elsewhere in the project — WAL's sequence `index`) stay 64-bit. The same rule should be
   applied to any new field added later, in this project or beyond (e.g. Phase 7's benchmark
   timers, Phase 9's replication sequence numbers — both belong in the "grows for the
   system's lifetime" bucket).
