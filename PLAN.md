# Matching Engine Journal & Replay — Plan

**Status:** v1 complete (Phases 0–6). Optional later: snapshots, mmap, event-journal mode.

**Scope lock:** Append-only **command** journal + deterministic replay around a vendored LOB. Single-threaded. Checksums for integrity. No TCP, no multi-threaded matching, no replication.

Docs: [README](README.md) · Theory: [`theory/index.html`](theory/index.html)

---

## Goal

Add the **persistence / recovery** slice missing from the LOB + MDS + gateway trilogy:

- Reuse matching (LOB); do not rebuild price-time rules.
- Durable log of what the matcher was told (`NEW` / `CANCEL`).
- Replay into a fresh engine → bit-identical events vs a live run.
- Practice serialization, append-only I/O, checksums, and golden recovery tests.

---

## Locked decisions (v1)

| Decision | Choice |
|----------|--------|
| Journal payload | Commands (inputs), not event-only log |
| On-disk format | `MEJR` header + type‖body‖CRC32 (LE packed) |
| LOB | Vendored slim copy in `include/lob/` |
| Threads | Single-threaded |
| Networking | Out of v1 |
| Snapshots / mmap | Out of v1 (optional later) |
| Dialect | C++17 |
| Write order (CLI) | Match-then-journal |
| Durability default | `fsync` on close; `--fsync-each` optional |

---

## Roadmap

| Phase | Goal | Status |
|-------|------|--------|
| 0 | README, PLAN, `.gitignore`, theory notes | ✅ |
| 1 | Vendored LOB; scenario load (reuse DSL) | ✅ |
| 2 | Journal record layout + writer (append, checksum) | ✅ |
| 3 | `journal_run`: scenario → match + journal file | ✅ |
| 4 | Journal reader + `journal_replay` rebuild | ✅ |
| 5 | Goldens: live vs replay identity; trunc/corrupt tests | ✅ |
| 6 | Docs polish; document `fsync` policy | ✅ (this pass; `--fsync-each` on CLI) |
| 7+ | Snapshots, mmap reader, event-journal mode | Optional |

---

## What v1 ships

- Binary append-only journal for `NEW` / `CANCEL`
- CLI to run a scenario while journaling (`journal_run`)
- CLI to replay a journal into a fresh `MatchingEngine` (`journal_replay`)
- Tests: live ≡ replay event lines (`identity_tests`)
- Tests: truncated or bad checksum → clear failure (`journal_tests`)
- Drop / unknown-symbol style counters are **out of scope** (gateway concern)

### Important design note (command vs event log)

Logging **commands** and rematching is enough when the engine is deterministic (same inputs ⇒ same trades). An **event** journal is a useful later mode for audit-of-outputs without rematch; not required for v1 recovery of book state.

---

## Out of scope (deliberate)

| Cut | Why |
|-----|-----|
| TCP / sockets | Covered by MDS |
| Threads / SPSC | Covered by gateway |
| Rewriting LOB matching | Covered by LOB repo |
| Raft / multi-node replication | Far beyond learning goal |
| Embedded SQL / kdb | Wrong tool for this lesson |
| Amend / FOK / icebergs | LOB feature surface |

---

## Optional next work

1. Periodic snapshots + replay from offset (faster recovery)
2. `mmap` for large journal reads
3. Dual-write: command journal + event journal
4. Hook journal writer behind gateway publisher path (cross-repo integration experiment)
5. Compare `fsync` every record vs batched flush (latency vs durability note)
6. Prefix salvage on bad tail (replay good records, report truncate) — v1 fails closed on whole file

---

## Theory reading order

| # | Note | Topic |
|---|------|--------|
| 1 | [`theory/Theory_Architecture.html`](theory/Theory_Architecture.html) | Paths, command journal, modules |
| 2 | [`theory/Theory_Journal_Format.html`](theory/Theory_Journal_Format.html) | Header + NEW/CANCEL records |
| 3 | [`theory/Theory_Durability_And_IO.html`](theory/Theory_Durability_And_IO.html) | Append, fsync, crash models |
| 4 | [`theory/Theory_Integrity.html`](theory/Theory_Integrity.html) | CRC, trunc, fail-closed |
| 5 | [`theory/Theory_Replay_And_Recovery.html`](theory/Theory_Replay_And_Recovery.html) | Rematch algorithm |
| 6 | [`theory/Theory_Testing_And_Correctness.html`](theory/Theory_Testing_And_Correctness.html) | Identity + negatives |
| 7 | [`theory/Theory_CodeRef.html`](theory/Theory_CodeRef.html) | C++ feature map |

## Code reading order

| # | File | Topic |
|---|------|--------|
| 1 | `include/lob/matching_engine.hpp` | Vendored match API |
| 2 | `include/command.hpp` / `scenario_loader.hpp` / `apply_command.hpp` | Host commands + DSL |
| 3 | `include/crc32.hpp` / `journal_record.hpp` | On-disk layout + CRC |
| 4 | `include/journal_writer.hpp` | Append + fsync |
| 5 | `include/journal_reader.hpp` | Sequential read / verify |
| 6 | `src/journal_run.cpp` | Live path |
| 7 | `src/journal_replay.cpp` | Recovery path |
| 8 | `tests/test_identity.cpp` / `test_journal_record.cpp` | Identity + corruption |

---

## Companion projects

| Repo | Role in the story |
|------|-------------------|
| Limit-Order-Book-and-Matching-Engine-Cpp | In-memory CLOB + goldens |
| TCP-Market-Data-Simulator-Cpp | Binary protocol on the wire |
| Concurrent-Order-Gateway-Cpp | Concurrent order pipeline |
| **This repo** | Durable log + deterministic rebuild |
