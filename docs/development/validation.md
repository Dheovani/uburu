# Product validation

This document defines the repeatable validation process used to move Uburu from preview releases toward a stable `1.0.0`. It complements automated tests and benchmarks: automated tests protect known behavior, benchmarks measure controlled workloads, and this process verifies the complete product against representative real-world data.

Only Windows and Linux are supported validation targets. Do not report a scenario as passed without recording the tested artifact or commit, platform, dataset profile, expected behavior, observed behavior, and relevant evidence.

## Validation record

Create one record under `docs/releases/` for each release candidate or other meaningful validation cycle. Use a name such as `v0.2.0-validation.md` or `v1.0.0-rc1-validation.md` and include:

- Uburu version and Git commit;
- operating system, version, architecture, and desktop environment when relevant;
- artifact or build preset used;
- CPU, logical thread count, installed memory, and storage type;
- dataset location category and profile without publishing private path or content;
- status for every applicable scenario: `Passed`, `Failed`, `Blocked`, or `Not applicable`;
- observed metrics, diagnostics report location, and linked issue for every failure.

Never commit private file names, full local paths, search expressions, document contents, or unmasked diagnostic output. A dataset identifier and aggregate characteristics are sufficient.

## Dataset profiles

Use data that can expose different failure modes. One directory may satisfy more than one profile, but the validation record must describe its approximate scale and composition.

| Profile | Required characteristics | Main risks covered |
| --- | --- | --- |
| Deterministic smoke | Small disposable directory with known matches, non-matches, nested paths, ignored paths, hidden files, and duplicate occurrences | Basic correctness, filtering, ordering, and repeatability |
| Code repository | Real Git repository with source files, generated output, `.gitignore`, Unicode paths, and enough history to switch branches safely | Git-aware traversal, ignore behavior, path handling, and branch reconciliation |
| Document corpus | Mix of plain text, PDF, DOCX, XLSX, PPTX, OpenDocument, RTF, HTML, subtitles, unsupported files, and malformed copies | Extractor correctness, name-only fallback, preview, safety limits, and partial failures |
| Many small files | Tens or hundreds of thousands of small files across a deep directory tree | Scanner throughput, queue pressure, cancellation, memory stability, and UI batching |
| Few large files | Multi-gigabyte aggregate data with files larger than normal source documents and matches near chunk boundaries | Streaming, bounded memory, offsets, long lines, cancellation, and progress reporting |
| Large mixed tree | A user-selected directory containing repositories, documents, binaries, hidden paths, inaccessible paths when available, and several gigabytes of data | End-to-end behavior under realistic heterogeneous load |

Private real-world datasets should remain outside the repository. Record only aggregate metadata such as approximate file count, total bytes, extension distribution, Git status composition, and whether storage was cold or warm.

## Automated gates before manual validation

Run the applicable quality gate before investigating product behavior manually. A release candidate cannot pass validation while its supported-platform build or tests are failing.

Windows MSVC core gate:

```powershell
cmake --preset core-windows-msvc-werror-debug
cmake --build --preset core-windows-msvc-werror-debug
ctest --preset core-windows-msvc-werror-debug
cmake --build build/core-windows-msvc-werror-debug --config Debug --target format-check
```

Linux core and sanitizer gates:

```sh
cmake --preset core-linux-werror-debug
cmake --build --preset core-linux-werror-debug
ctest --preset core-linux-werror-debug
cmake --build build/core-linux-werror-debug --target format-check

cmake --preset core-linux-sanitize-debug
cmake --build --preset core-linux-sanitize-debug
ctest --preset core-linux-sanitize-debug
```

ThreadSanitizer should be run when concurrency, cancellation, queues, watchers, indexing publication, or result delivery changes:

```sh
cmake --preset core-linux-tsan-debug
cmake --build --preset core-linux-tsan-debug
ctest --preset core-linux-tsan-debug
```

On Windows, run the selected stable-release performance scenarios in Release mode with repeated median-based guardrails:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run-benchmark-validation.ps1
```

On Linux, pass the corresponding configured benchmark build directory with `-BuildDirectory`. The gate validates throughput, time to first result, regex JIT, batching, index reuse, branch-switch behavior, and bounded queue occupancy before manual large-dataset observations begin.

## Correctness scenarios

Run these scenarios through the desktop application and repeat the central search cases through the CLI. Compare results by file, occurrence, line, column, and completion state rather than only by the visible result count.

| Area | Scenario | Acceptance criteria |
| --- | --- | --- |
| Literal matching | Search known expressions at the start, middle, and end of lines, including repeated and overlapping candidates | Every occurrence follows [search semantics](../product/search-semantics.md); ordering is deterministic across repeated runs |
| Case and boundaries | Repeat with case-sensitive, case-insensitive, whole-word, identifier, punctuation, and Unicode cases | Results match the documented Unicode and boundary rules without platform differences |
| Regex | Run valid selective and broad expressions, then invalid and pathological expressions | Valid results are correct; invalid patterns produce typed errors; resource limits and cancellation remain responsive |
| File names and types | Search content and file names across supported, unsupported, binary, and filtered extensions | Every scanned file remains name-searchable; type filters do not leak unrelated extensions; unsupported content is reported accurately |
| Scope | Exercise root-only, recursive, multiple roots, explicit inclusions, exclusions, hidden files, and `.gitignore` | No result escapes the selected scope; include/exclude precedence matches the documented rules |
| Encodings and lines | Include UTF-8, UTF-16 LE/BE, Latin-1 fallback, LF, CRLF, standalone CR, empty files, and files without a final newline | Text, offsets, line numbers, columns, and highlights remain correct |
| Rich documents | Search known text in every supported document format and preview the selected occurrence | Extracted text is searchable; location labels are useful; preview opens at the occurrence; malformed or protected files cannot abort the search |
| Partial failures | Include inaccessible, removed-during-read, malformed, unsupported, and safety-limited files | Valid results are preserved; summary and UI distinguish skipped files, partial failures, and fatal errors |
| Cancellation | Cancel during scan, extraction, matching, result delivery, initial indexing, and incremental indexing | Cancellation becomes visible promptly, no late result corrupts current state, and the last published index remains usable |
| Direct/indexed/hybrid | Run the same query directly, after a fresh index, after file changes, and with a deliberately stale index | Final visible results converge to the current filesystem state with deterministic merge and no stale deleted result |

The equivalent CLI smoke command is:

```sh
uburu search <root> <expression> --strategy direct --format jsonl
```

Repeat with `--strategy indexed` and `--strategy hybrid` after rebuilding the index:

```sh
uburu index-rebuild <root> --format jsonl
uburu index-status <root> --format jsonl
```

JSON Lines output should be captured when a desktop result is disputed because it separates engine/service behavior from QML rendering.

### Privacy-safe automated evidence

After choosing a dataset with a known matching expression, use the portable CMake validation runner to exercise direct, indexed, and hybrid search plus initial/incremental indexing. The runner invokes the CLI with `--summary-only`, keeps its disposable database under the ignored `build/product-validation-private` directory, removes it after the run, and writes only aggregate evidence:

```powershell
cmake `
  -DUBURU_EXECUTABLE=build/windows-msvc-debug/apps/cli/Debug/uburu.exe `
  -DUBURU_DATASET_ROOT=C:/private/dataset `
  -DUBURU_EXPRESSION=known-private-expression `
  -DUBURU_DATASET_ID=windows-large-mixed-01 `
  -DUBURU_DATASET_PROFILE=large-mixed-tree `
  -DUBURU_CONFIGURATION=windows-msvc-debug `
  -DUBURU_OUTPUT=docs/releases/v1.0.0-rc1-windows-large-mixed.md `
  -P scripts/run-product-validation.cmake
```

The same command works on Linux with the Linux `uburu` executable and shell-appropriate quoting. Optional inputs are `UBURU_TYPES`, `UBURU_INCLUDE_BINARY`, `UBURU_INCLUDE_SUBDIRECTORIES`, `UBURU_ALLOW_PARTIAL_FAILURE`, `UBURU_MINIMUM_MATCHES`, `UBURU_MAX_SIZE_MIB`, `UBURU_MEMORY_BUDGET_MIB`, and `UBURU_THREADS`. Binary files are excluded, subdirectories are included, partial failures are rejected, at least one match is required, and the maximum file size is 16 MiB by default. Set `UBURU_MAX_SIZE_MIB` high enough to cover the largest intended fixture; the same value is applied to direct search and index rebuilding and is recorded in the evidence. `UBURU_ALLOW_PARTIAL_FAILURE=ON` is appropriate for explicitly mixed or hostile corpora where unsupported documents are expected; it still rejects cancellation, resource exhaustion, missing summaries, and strategy divergence. Product evidence must use an optimized Release executable because Debug hashing and extraction timings are not representative; `UBURU_REQUIRE_RELEASE_BUILD=OFF` may be used only for preliminary diagnostics. Supported profile identifiers are `deterministic-smoke`, `code-repository`, `document-corpus`, `many-small-files`, `few-large-files`, and `large-mixed-tree`.

Formal evidence requires a clean Git worktree and an optimized Release build by default, ensuring that the recorded commit identifies the tested source exactly and that timings represent the shipped configuration. During development, `-DUBURU_REQUIRE_CLEAN_WORKTREE=OFF` or `-DUBURU_REQUIRE_RELEASE_BUILD=OFF` may be used for a preliminary diagnostic run, but its record must not be promoted to release evidence.

The generated record compares final match counts, requires a fresh index, rejects undeclared partial or indexing failures, cancellation, result-limit exhaustion, and memory-limit exhaustion, and records aggregate timing, throughput, queue, memory, reuse, and document-extraction status counters. Extraction counters distinguish completed, unsupported, safety-limited, protected, open/read failure, invalid-encoding, and parser-failure outcomes without retaining private paths. It deliberately cannot validate visual responsiveness, preview behavior, cancellation latency, partial-failure presentation, branch switching, or clean-machine artifacts; those scenarios remain manual responsibilities.

### Repeatable cancellation evidence

Use the cancellation runner to verify that a real direct-search workload observes cooperative cancellation within a bounded interval. It schedules cancellation through the same `std::stop_token` path used by `Ctrl+C`, requires stable exit code `4`, rejects a missing cancellation summary, and records only aggregate counters:

```powershell
cmake `
  -DUBURU_EXECUTABLE=build/windows-msvc-release/apps/cli/Release/uburu.exe `
  -DUBURU_DATASET_ROOT=C:/private/dataset `
  -DUBURU_EXPRESSION=known-private-expression `
  -DUBURU_DATASET_ID=windows-cancellation-01 `
  -DUBURU_DATASET_PROFILE=document-corpus `
  -DUBURU_CONFIGURATION=windows-msvc-release `
  -DUBURU_OUTPUT=docs/releases/v1.0.0-rc1-windows-cancellation.md `
  -DUBURU_CANCELLATION_DELAY_MILLISECONDS=250 `
  -DUBURU_MAXIMUM_COMPLETION_MILLISECONDS=2000 `
  -P scripts/run-cancellation-validation.cmake
```

Optional scope inputs match the product-validation runner: `UBURU_TYPES`, `UBURU_INCLUDE_BINARY`, `UBURU_INCLUDE_SUBDIRECTORIES`, `UBURU_MAX_SIZE_MIB`, `UBURU_MEMORY_BUDGET_MIB`, and `UBURU_THREADS`. This automated gate measures deterministic end-to-end cancellation in the CLI. A release cycle must still include physical `Ctrl+C` and desktop Cancel-button observations because operating-system console delivery and UI responsiveness are outside the scheduled-deadline test.

## Git and incremental-index scenarios

Use a disposable clone or a repository where branch and file changes are safe. Record the initial branch, HEAD, worktree count, and aggregate working-tree state without exposing private names.

1. Build an initial index on a clean branch and confirm indexed results.
2. Modify, add, delete, rename, ignore, and conflict representative files, then verify that search reflects the visible working tree.
3. Reindex without changes and confirm catalog, content-hash, or blob-hash reuse is nonzero where applicable.
4. Switch to a branch that adds, removes, renames, and reuses content, then verify staleness detection and incremental reconciliation.
5. Repeat from detached HEAD.
6. Validate a secondary worktree and confirm that its identity, overlay, and results do not leak into the original worktree.
7. Validate nested repositories and submodules according to the documented traversal policy.
8. Interrupt an index update, restart Uburu, and confirm that the last published generation and preferences remain valid.

## Performance and resource observations

Correctness takes priority, but each medium or large dataset run must record enough information to expose regressions:

- startup latency;
- time to first result and total search duration;
- scanned files, read files, processed bytes, matches, ignored files, binary skips, unsupported formats, and extraction failures;
- files and bytes per second;
- approximate peak memory and whether memory returns to a stable range across repeated searches;
- cancellation latency observed by the user;
- initial and incremental indexing duration;
- catalog, content-hash, and blob-hash reuse;
- database and cache size before and after indexing;
- preview latency for plain text, large files, and rich documents;
- UI responsiveness while batches and indexing progress are active.

Use [performance.md](performance.md) for quantitative targets and benchmark methodology. A manual run outside a target is a finding to investigate, not permission to silently weaken the target.

## Acceptance rules

A validation cycle passes only when:

- automated gates pass on every supported platform affected by the release;
- every P0 correctness scenario passes on Windows and Linux;
- no crash, assertion, data corruption, out-of-scope result, unbounded memory growth, UI freeze, or stale final result remains unexplained;
- cancellation preserves usable application and index state;
- failures in individual files remain bounded and visible without invalidating correct results;
- performance observations have no unexplained order-of-magnitude regression from the relevant baseline;
- every known exception is documented in the validation record and release notes with an issue or explicit risk decision.

A blocked scenario is not a pass. A scenario may be `Not applicable` only when the feature or platform is outside the supported release scope.

## Result template

Copy this table into the release-specific validation record and expand it with the applicable scenarios above:

| Scenario | Platform | Dataset profile | Strategy | Status | Key observations | Evidence or issue |
| --- | --- | --- | --- | --- | --- | --- |
| Example: recursive literal search | Windows | Large mixed tree | Direct | Not run | — | — |

Finish the record with the tested version and commit, machine profile, artifact hashes when applicable, unresolved blockers, accepted limitations, and the final release decision.
