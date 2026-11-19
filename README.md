# Matching Engine Journal & Replay (C++)

Append-only **command journal** and **deterministic replay** for a price-time limit order book. Log `NEW` / `CANCEL` commands, rebuild book state after restart or crash, verify bit-identical results against golden scenarios, and detect truncated or corrupt tails with checksums.

Companion to the [Limit Order Book](https://github.com/iliarusskikh/Limit-Order-Book-and-Matching-Engine-Cpp) (matching rules), [TCP Market Data Simulator](https://github.com/iliarusskikh/TCP-Market-Data-Simulator-Cpp) (wire I/O), and [Concurrent Order Gateway](https://github.com/iliarusskikh/Concurrent-Order-Gateway-Cpp) (threads / SPSC). **No third-party libraries** beyond the C++ standard library (CMake to build).

**Scope (v1):** Persistence and recovery around a vendored LOB. Single-threaded journal + replay. No TCP, no multi-threaded matching.

| Doc | Purpose |
|-----|---------|
| [`theory/index.html`](theory/index.html) | Knowledge notes (format, durability, CRC, replay) |
| [`LICENSE`](LICENSE) | MIT |

---

## What it does

Commands that drive the matcher are appended to a durable log. After exit, crash, or a fresh process start, **replay** reads the log and feeds a clean `MatchingEngine`. The rebuilt book and event stream must match a live run. Checksums catch truncated or corrupt tails.

```text
  scenario.txt
        │
        ▼
  ┌─────────────┐     append      ┌──────────┐
  │  Matcher    │ ──────────────► │  *.jnl   │
  │  (LOB)      │                 │ append-  │
  └─────────────┘                 │ only +   │
        │                         │ CRC32    │
        │ live events             └────┬─────┘
        ▼                              │ replay
     stdout                     fresh MatchingEngine
                                → same events / book
```

Focus: **serialize → store → rebuild → prove identical** — not networking or concurrency.

---

## Scope lock (v1)

| In | Out |
|----|-----|
| Append-only binary command journal | TCP / sockets |
| Replay → rebuild LOB from genesis | Multi-threaded gateway |
| Golden identity tests (live vs replay) | Replication / consensus |
| Per-record CRC32 / trunc detect | GUI, FIX, embedded DB |
| Vendored LOB as black box | Rewriting matching rules |
| CLI: `journal_run`, `journal_replay` | Snapshots / mmap (optional later) |

---

## Design decisions

| Topic | Choice | Why |
|-------|--------|-----|
| Journal contents | **Commands** (`NEW` / `CANCEL`), not events | Deterministic rematch; mirrors LOB scenario DSL |
| Format | Packed LE records: type ‖ body ‖ CRC32; file magic `MEJR` | Same framing discipline as MDS; portable |
| Flush policy | `fsync` on close; optional `--fsync-each` | Teach durability vs throughput |
| Write order | Match-then-journal (scenario CLI) | Only log commands that were applied |
| LOB | Vendored slim copy under `include/lob/` | Standalone build; matching stays a black box |
| Concurrency | Single-threaded | Persistence lesson without queue noise |
| Dialect | C++17 | Filesystem, chrono, fixed-width ints, POSIX `fsync` |

---

## C++ concepts in this tree

| Concept | Where |
|---------|--------|
| Binary serialization | `journal_record.hpp` encode/decode |
| Append-only file I/O | `journal_writer.hpp` (POSIX fd) |
| Checksums / integrity | `crc32.hpp` + record trailer; `journal_reader.hpp` |
| RAII file handles | Writer closes/`fsync`s on destruction |
| Deterministic rebuild | `journal_replay.cpp` + `tests/test_identity.cpp` |
| Vendored library boundary | `include/lob/` |

---

## Project structure

```
├── CMakeLists.txt
├── LICENSE
├── README.md
├── theory/              # HTML knowledge notes
├── include/
│   ├── lob/             # vendored matching engine
│   ├── command.hpp
│   ├── scenario_loader.hpp
│   ├── apply_command.hpp
│   ├── crc32.hpp
│   ├── journal_record.hpp
│   ├── journal_writer.hpp
│   └── journal_reader.hpp
├── src/
│   ├── journal_run.cpp
│   └── journal_replay.cpp
├── tests/
│   ├── test_phase1_lob.cpp
│   ├── test_journal_record.cpp
│   └── test_identity.cpp
└── scenarios/           # LOB-style DSL (NEW/CANCEL; CHECKs ignored by loader)
```

## Requirements

- C++17, CMake 3.16+, g++/clang++ (Linux / macOS / WSL)

## Build & run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure

./build/journal_run scenarios/g2_exact_fill.txt -o /tmp/run.jnl
./build/journal_replay /tmp/run.jnl
# optional: ./build/journal_run … -o /tmp/run.jnl --fsync-each
```

| Binary | Role |
|--------|------|
| `journal_run` | Scenario → match + write `.jnl` |
| `journal_replay` | Read `.jnl` → rematch → print events / best bid-ask |
| `phase1_tests` | Vendored LOB + scenario loader smoke |
| `journal_tests` | CRC vector, round-trip, trunc / bad checksum |
| `identity_tests` | Live event lines ≡ replay |

---

## Related projects

| Project | Focus |
|---------|--------|
| LOB matcher (C++) | Price-time book, events, golden tests — in-memory |
| TCP MDS (C++) | Sockets, framing, `poll`, heartbeats |
| Concurrent gateway (C++) | Threads, SPSC, sharding, backpressure |
| **This journal (C++)** | Append-only log, replay, checksums, recovery |
