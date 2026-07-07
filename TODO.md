# TODO — Uburu

This document is the project's operational plan. Milestone order represents real dependencies: later items must not degrade correctness, cancellation, memory use, or core/UI separation just to move faster.

## Backlog conventions

- `[ ]` pending; `[x]` completed and validated.
- `P0` blocks a trustworthy base; `P1` forms the main product; `P2` professionalizes and expands it; `P3` is advanced evolution.
- An item may be marked as completed only when code, tests, documentation, and applicable metrics are updated.
- Critical core changes require automated tests. Performance changes require before/after benchmarks or a reproducible metric.
- Visible text must exist in `pt-BR` and `en-US`.

## Current validated state

- [x] Qt-independent `uburu_core` library.
- [x] Application layer separated from UI.
- [x] Minimal Qt Quick application with asynchronous search and cancellation.
- [x] Initial recursive scanner and line-by-line literal search.
- [x] Central types for repository, worktree, documents, and results.
- [x] Initial search, filesystem, index, Git, and storage interfaces.
- [x] Search scope with multiple roots and per-root exclusions in the core.
- [x] C++23 build with CMake, Qt 6.11.1/MSVC, and vcpkg.
- [x] Initial unit tests with Catch2.
- [x] Automatic formatting and kebab-case convention.

## Milestone 0 — Reproducible engineering and repository hygiene (P0)

- [x] Initialize and document the Git repository, main branch, and commit policy.
- [x] Add `CMakePresets.json` for Windows/MinGW, Windows/MSVC, Linux, and core-only development.
- [x] Remove commands dependent on fixed local paths such as `C:\Qt\6.11.1`.
- [x] Fix `README.md` and `docs/build.md` with the currently validated MSVC flow.
- [x] Add scripts or presets to configure, build, test, format, and run the application.
- [x] Create a local deployment target or script with `windeployqt` and vcpkg DLLs.
- [x] Add `.editorconfig` consistent with `.clang-format` and Markdown/QML files.
- [x] Add static analysis with clang-tidy and configure the initial rule set.
- [x] Add warnings-as-errors validation in CI, with compiler-specific exceptions justified.
- [x] Add sanitizers on compatible platforms: AddressSanitizer and UndefinedBehaviorSanitizer.
- [x] Add automatic formatting verification without modifying files (`format-check`).
- [x] Create CI for Windows, Linux, and macOS when feasible.
- [x] Run configure, build, tests, format-check, and static analysis in CI.
- [x] Define minimum-version policy for CMake, Qt, compilers, and vcpkg.
- [x] Pin the vcpkg baseline for reproducible builds.
- [x] Add `LICENSE`, `CONTRIBUTING.md`, code of conduct, and security policy.
- [x] Document dependency licenses and Qt redistribution obligations.

### Exit criteria

- [x] A clean clone can be configured and tested through presets without manually editing paths.
- [x] CI green on at least Windows/MinGW and Linux.

## Milestone 1 — Correct direct-search semantics (P0)

- [x] Formally specify semantics in `docs/search-semantics.md`.
- [x] Validate `SearchQuery` and return typed errors for invalid root, empty expression, and incompatible options.
- [x] Find all occurrences in a line, not only the first one.
- [x] Define and test overlapping occurrence behavior.
- [x] Implement Unicode-consistent case-sensitive and case-insensitive search.
- [x] Implement whole word with Unicode rules and a specific option for code identifiers.
- [x] Implement regex search with PCRE2.
- [x] Enable PCRE2 JIT when supported and provide explicit fallback.
- [x] Limit regex time, depth, and resources to avoid pathological patterns.
- [x] Return regex compilation errors with position and translatable message.
- [x] Implement file-name search separately from content search.
- [x] Implement filters by glob, extension, directory, and size.
- [x] Normalize extensions and define platform-specific case sensitivity.
- [x] Implement include/exclude with documented precedence.
- [x] Apply global and per-file result limits.
- [x] Implement deterministic ordering and an initial relevance strategy.
- [x] Preserve progressive results without waiting for traversal completion.
- [x] Distinguish cancellation, partial failure, and normal completion in the summary.
- [x] Propagate read errors without silently stopping the whole search.
- [x] Define behavior for files changed or removed during reading.
- [x] Avoid unnecessary copies of lines, paths, and results.

### Required tests

- [x] Literal search with multiple occurrences on the same line.
- [x] Case-sensitive and case-insensitive search with ASCII and Unicode.
- [x] Whole word, identifiers, punctuation, and Unicode boundaries.
- [x] Valid regex, invalid regex, JIT/fallback, and cancellation.
- [x] CRLF, LF, file without final newline, and empty lines.
- [x] Result limits, filters, and deterministic ordering.
- [x] Permission errors, removed files, and partial reads.

## Milestone 2 — Text, encoding, and large files (P0)

- [x] Create a streaming text-reader abstraction in `core/text`.
- [x] Detect BOM and support UTF-8, UTF-16 LE, and UTF-16 BE.
- [x] Define configurable fallback for Latin-1 and unknown encoding.
- [x] Validate UTF-8 and define an explicit policy for invalid sequences.
- [x] Detect binaries with robust sampling, not only per-line NUL bytes.
- [x] Make sample size and binary policy configurable.
- [x] Read large files in chunks without losing matches at boundaries.
- [x] Preserve correct offsets between bytes, code points, lines, and visual columns.
- [x] Extract previous/following context without loading the whole file.
- [x] Produce highlight spans for multiple occurrences.
- [x] Support LF, CRLF, and standalone CR in documented form.
- [x] Define a limit for extremely long lines.

### Exit criteria

- [x] Search files larger than the memory budget without allocation proportional to file size.
- [x] Deterministic fixtures cover all supported encodings and line endings.

## Milestone 3 — Filesystem, ignore, and concurrency (P0)

- [x] Implement real `.gitignore` with nested rules, negation, and precedence.
- [x] Support global Git ignore files and `.git/info/exclude` when configured.
- [x] Separate hidden, ignored, and binary files in metrics.
- [x] Correctly apply included and excluded directories in the scanner.
- [x] Normalize absolute and relative paths per platform.
- [x] Handle long paths, UNC, and Windows case differences.
- [x] Detect sparse files and define per-platform reading policy.
- [x] Define policy for junctions, symlinks, and mount points.
- [x] Detect cycles when following symlinks.
- [x] Implement a worker pool with configurable size.
- [x] Prioritize small files and likely candidates without breaking final determinism.
- [x] Add a bounded queue and backpressure between scan, read, matching, and publication.
- [x] Ensure cooperative cancellation in scan, queue, reading, and matching.
- [x] Avoid a global mutex and measure contention.
- [x] Implement watchers behind a common interface.
- [x] Implement Windows backend with `ReadDirectoryChangesW`.
- [x] Implement Linux backend with `inotify`.
- [x] Implement macOS backend with `FSEvents`.
- [x] Handle overflow/lost events with reconciliation rescan.
- [x] Add a documented initial fallback when native backend is unavailable.

## Milestone 4 — Complete Git integration (P1)

- [x] Implement `GitService` with libgit2 and typed errors.
- [x] Discover common repository, `.git` file/directory, and worktree root.
- [x] Generate stable identifiers for repository and worktree.
- [x] Detect current branch, HEAD, and detached HEAD.
- [x] Enumerate multiple worktrees.
- [x] Read tracked, untracked, ignored, modified, deleted, and conflicted files.
- [x] Obtain blob OID for tracked files.
- [x] Detect changes in HEAD, index, and relevant refs.
- [x] Treat branch switch as incremental structural reconciliation.
- [x] Model local overlay over versioned content.
- [x] Handle renames and moves with content reuse.
- [x] Define behavior for submodules and nested repositories.
- [x] Support locked, removed, and prunable worktrees.
- [x] Isolate Git CLI fallback behind an explicit adapter.
- [x] Test SHA-1 repositories and prepare types for SHA-256.

### Git integration tests

- [x] Normal branch and detached HEAD.
- [x] Branch switch with added, removed, and blob-identical files.
- [x] Locally modified, new, deleted, ignored, and conflicted files.
- [x] Multiple worktrees and submodule.
- [x] Content reuse by blob/hash between branches and worktrees.

## Milestone 5 — Professional SQLite storage (P1)

- [x] Implement `StorageService` with RAII and prepared statements.
- [x] Create a versioned migration system.
- [x] Define schema for repositories, worktrees, generations, files, documents, and overlays.
- [x] Separate path identity from content identity.
- [x] Store content hash and blob hash with explicit algorithm/versioning.
- [x] Enable WAL, foreign keys, busy timeout, and measured pragmas.
- [x] Publish index generations in atomic transactions.
- [x] Recover from interruption during migration or indexing.
- [x] Validate integrity and provide safe rebuild of corrupted index.
- [x] Implement retention and orphan-document collection.
- [x] Persist global and per-repository preferences.
- [x] Persist search history and saved searches.
- [x] Persist indexing metrics without unbounded growth.
- [x] Define default database location per platform.
- [x] Allow custom database location and migration.
- [x] Test concurrency between search reads and new-generation writes.
- [x] Evaluate FTS5 by benchmark without coupling the contract to the backend.

## Milestone 6 — Persistent and incremental index (P1)

- [x] Define and version the internal indexed-document format.
- [x] Choose content hash with throughput benchmark and acceptable collision risk.
- [x] Implement cancellable and progressive initial indexing.
- [x] Implement incremental catalog by size, mtime, hash, and Git state.
  - [x] Reuse catalog entry when size, mtime, persisted hash, and clean status indicate unchanged file.
  - [x] Integrate Git state/overlay into the incremental catalog before considering the item complete.
- [x] Deduplicate documents by content hash.
- [x] Expose storage reuse queries by content hash.
- [x] Reuse documents by blob hash before rereading files.
- [x] Expose storage reuse queries by Git blob hash.
- [x] Apply working tree overlay over the versioned generation.
  - [x] Treat Git `modified` status as invalidation of conservative catalog reuse.
  - [x] Publish tombstones for locally deleted files when a previous document exists.
  - [x] Translate `GitOverlayEntry` values into testable indexing candidates and tombstones.
  - [x] Connect overlay to the incremental `IndexService` pipeline.
  - [x] Orchestrate `GitService::workingTreeOverlay()` in the application/indexing service.
- [x] Hide deleted files and replace modified files without stale results.
- [x] Reconcile watcher events in transactional batches.
- [x] Detect index staleness and expose state through `IndexService`.
- [x] Implement indexed search by content and metadata.
  - [x] Implement initial indexed search by path metadata.
  - [x] Persist and query indexed content without depending only on hash.
- [x] Combine fast index results with direct validation.
- [x] Update, confirm, or remove results during refinement.
- [x] Define deterministic ranking and merge between sources.
- [x] Implement disk budget and eviction policy.
- [x] Version schema and format for upgrades without unnecessary cache loss.
- [x] Implement pause, resume, and manual reindexing.

## Milestone 7 — Search service and observability (P1)

- [x] Make `SearchService` choose direct, indexed, or hybrid search by explicit policy.
- [x] Separate application DTOs from persistence types and engine details.
- [x] Create an event channel for progress, results, corrections, and errors.
- [x] Add search IDs to discard late events from cancelled queries.
- [x] Implement adaptive result batching for the UI.
- [x] Measure time to first result and total time in all strategies.
- [x] Implement a concrete `MetricsSink` and structured logging.
- [x] Add levels, categories, and log rotation.
- [x] Remove or mask sensitive content and paths from logs by default.
- [x] Measure files/bytes per second, queues, cache hits, and hash reuse.
- [x] Measure approximate memory and detect growth between searches.
- [x] Create an exportable diagnostics mode/screen.
- [x] Add search tracing with no relevant penalty when disabled.

## Milestone 8 — Productive desktop experience (P1)

- [x] Redesign the main screen with responsive layout and clear empty states.
- [x] Implement directory/repository selector with recent entries and favorites.
- [x] Allow selecting multiple directories/repositories in the visual selector, with per-root subdirectory inclusion and exclusion.
  - [x] Allow multiple selected roots on desktop and execute search over `SearchScope`.
  - [x] Display selected roots as removable chips in the visual selector.
  - [x] Allow configuring per-root subdirectory exclusions.
  - [x] Allow configuring explicit per-root subdirectory inclusions.
- [x] Expose all planned filters without hardcoded text.
- [x] Add configurable debounce and search as you type.
- [x] Show count, processed files, time to first result, and total duration.
- [x] Show indexing status and progress.
  - [x] Reserve a visible UI point for indexing state.
  - [x] Connect real `IndexingService` progress to desktop.
- [x] Virtualize the list for hundreds of thousands of results.
- [x] Preserve selection during batches and hybrid refinement.
- [x] Create grouping by file and navigation between occurrences.
- [x] Implement asynchronous, cancellable, bounded file preview.
- [x] Implement multi-occurrence highlight and context lines.
- [x] Add line numbers, monospace font, and configurable tab width.
- [x] Open file in the configured editor and copy path/occurrence.
- [x] Show an Uburu-owned file action menu on right click over a listed file, with actions equivalent to common system actions: open file, open with when available, open file location, copy path, and copy occurrence.
- [x] Customize/improve the visual style of the action menu.
- [x] Add essential shortcuts and initial command palette.
- [x] Complete command palette with diagnostics, history, saved searches, and advanced navigation.
- [x] Implement history, saved searches, and favorites.
- [x] Implement light, dark, and system themes.
- [x] Add information icons with tooltips explaining filters, scope, document types, `.gitignore`, regex search, and other potentially ambiguous controls.
- [x] Fix interface language inconsistencies, ensuring all visible text is in `pt-BR` or `en-US` according to the active language, without accidental mixing.
- [x] Persist geometry, splitters, filters, and last repository.
- [x] Show partial errors without interrupting valid results.
- [x] Prevent regex search while the backend is unavailable or remove the misleading visual stub.
- [x] Make cancellation immediate and visually reliable.
- [x] Test and document initial accessibility: focus, keyboard, contrast, accessible names, and screen readers.
- [x] Test and document responsiveness for high DPI, multiple monitors, and fractional scales.
- [x] Complete and review `pt-BR` and `en-US` translations.
- [x] Define strategy for pluralization, shortcuts, and technical strings.

## Milestone 9 — Tests and continuous quality (P0/P1)

- [x] Create RAII helpers for temporary directories and files in tests.
- [x] Remove fixed temporary names that may collide in parallel execution.
- [x] Create small fixtures for text, encoding, ignore, and Git.
- [x] Add unit tests for each pure matching and filtering rule.
- [x] Add scanner integration tests in a real temporary filesystem.
- [x] Add SQLite integration tests with disposable database.
- [x] Add libgit2 integration tests with disposable repositories.
- [x] Test cancellation at different pipeline points.
- [x] Test backpressure and memory limits.
- [x] Test concurrency repeatedly and under ThreadSanitizer where available.
- [x] Add Qt tests for controller/model and observable UI states.
- [x] Add a few end-to-end tests for selecting folder, searching, cancelling, and opening result.
- [x] Enable safe parallel CTest execution.
- [x] Configure per-module coverage and publish report in CI.
- [x] Define thresholds by critical behavior, without chasing cosmetic coverage.
- [x] Create regression suite with real bugs found.
- [x] Add fuzzing for matcher, ignore parser, encoding, and paths.

## Milestone 10 — Benchmarks and performance targets (P1)

- [x] Choose benchmark framework and integrate it into CMake without affecting the default build.
- [x] Create deterministic dataset generator.
- [x] Measure many small files and few large files.
- [x] Measure literal case-sensitive, case-insensitive, whole word, and regex/JIT.
- [x] Measure optional Unicode normalization cost before enabling it on the hot path.
- [x] Measure time to first result separately from total time.
- [x] Measure cold/hot scan and operating-system cache effects.
- [x] Measure initial indexing, incremental indexing, and branch switch.
- [x] Measure reuse by content hash and blob hash.
- [x] Measure memory for queues, results, and index.
- [x] Measure batching and UI rendering cost.
- [x] Define baselines by hardware/dataset and store versioned results.
- [x] Create relevant regression alerts in CI or periodic execution.
- [x] Document quantitative targets per repository class.

## Milestone 11 — Settings, privacy, and resilience (P2)

- [x] Implement typed and versioned global settings.
- [x] Implement per-repository settings with predictable inheritance.
- [x] Separate real indexing failures from files ignored by unsupported format, binary, size, filter, or temporary parser limitation in indexing status.
- [ ] Add determinate or indeterminate bottom progress bars for active search and indexing, using exact progress when the total work is known and a clearly marked ongoing state when scanning still cannot estimate the remaining work.
- [x] Validate limits for threads, files, results, memory, and disk.
- [ ] Add import/export for settings and saved searches.
- [x] Define telemetry policy: disabled by default and opt-in only, if it exists.
- [ ] Provide window settings buttons for language, theme, general preferences, and quick diagnostics.
  - [x] Add a compact top-left application menu as the entry point for settings and common commands.
  - [ ] Connect language and general-preferences actions to real settings screens.
- [x] Never send names, paths, or content without explicit consent.
- [x] Protect history and index according to user permissions.
- [x] Handle inaccessible paths, removable media, and unstable network locations.
- [ ] Recover state after crash without corrupting index or preferences.
- [ ] Implement local, exportable crash reports.
- [ ] Add limits against decompression bombs and special formats when supported.
- [x] Create a threat model for regex, hostile files, symlinks, and local database.

## Milestone 12 — Document extractors and rich file formats (P1)

- [ ] Define a document-extraction interface independent from `core/text`, so direct search, indexing, preview, and future CLI can consume extracted text without coupling the core to one parser library.
- [ ] Preserve file-name search for every scanned file even when content extraction is unavailable, unsupported, skipped, or fails.
- [ ] Add safe text extraction for PDF files, including page-aware result locations, bounded memory use, cancellation, encrypted/protected-file handling, malformed-file errors, and regression fixtures.
- [ ] Add safe text extraction for DOCX files through the OOXML package structure, including paragraph/table text, basic metadata, decompression limits, cancellation, unsupported feature reporting, and regression fixtures.
- [ ] Add safe text extraction for XLSX files through the OOXML package structure, including shared strings, sheet names, cell text, bounded worksheet traversal, decompression limits, cancellation, and regression fixtures.
- [ ] Add safe text extraction for PPTX files through the OOXML package structure, including slide text, speaker notes when feasible, slide-aware result locations, decompression limits, cancellation, and regression fixtures.
- [ ] Evaluate support for legacy Microsoft Office formats (`.doc`, `.xls`, `.ppt`) behind an optional extractor or explicit dependency decision, because binary Office parsing is higher risk than OOXML.
- [ ] Evaluate support for OpenDocument formats (`.odt`, `.ods`, `.odp`) using the same archive-safety model planned for OOXML.
- [ ] Add support for RTF extraction, with explicit limits for nested groups, escaped text, embedded objects, and malformed documents.
- [ ] Add support for HTML/XHTML extraction as structured text, excluding scripts/styles by default and preserving visible text semantics for search and preview.
- [ ] Add support for common subtitle and transcript formats (`.srt`, `.vtt`) as first-class text documents, including time-aware result locations when practical.
- [ ] Evaluate email/message formats (`.eml`, `.msg`) with privacy-safe attachment handling and no automatic traversal into attachments until limits and UX are defined.
- [ ] Define a user-visible unsupported-format policy that distinguishes "name-only searchable", "content extractor unavailable", "content extraction failed", and "content extraction skipped by safety limits".
- [ ] Add extractor-specific metrics for files processed, bytes processed, extraction time, skipped unsupported files, skipped unsafe archives, parser failures, and indexed extracted text size.
- [ ] Add fuzzing and hostile-file tests for document extractors, especially archive containers, malformed PDFs, malformed RTF, oversized shared strings, and nested/recursive package structures.
- [ ] Document supported formats, limitations, dependencies, and security boundaries in `docs/search-semantics.md`, `docs/indexing.md`, and `docs/privacy.md`.

## Milestone 13 — CLI and extensibility (P2)

- [ ] Create a thin CLI over the same `SearchService`, without duplicating the engine.
- [ ] Support human output, JSON Lines, and stable exit codes.
- [ ] Allow direct search, indexed search, and index status/rebuild through CLI.
- [ ] Keep signal cancellation and streaming with backpressure.
- [ ] Define interfaces for symbol and language parsers.
- [ ] Evaluate tree-sitter for symbols only behind a replaceable adapter.
- [ ] Define internal API for new index backends and watchers.
- [ ] Version public contracts before allowing external plugins.
- [ ] Document ABI/API stability limits.

## Milestone 14 — Packaging and releases (P2)

- [ ] Automate Windows bundle with Qt, MinGW/MSVC runtime, and required vcpkg DLLs.
- [ ] Produce Windows installer and evaluate MSIX versus traditional installer.
- [ ] Sign release executables and installers.
- [ ] Produce macOS bundle, sign, and notarize.
- [ ] Produce AppImage and evaluate Flatpak on Linux.
- [ ] Validate installation, update, and uninstallation on clean machines.
- [ ] Separate settings, index, and cache to allow safe upgrade/uninstall.
- [ ] Define semantic versioning and changelog.
- [ ] Automate release artifacts and checksums.
- [ ] Generate SBOM and license report.
- [ ] Add update policy and stable/pre-release channel.
- [ ] Create release, rollback, and database compatibility checklist.

## Milestone 15 — Professional documentation (P1/P2)

- [ ] Create `docs/search-semantics.md`.
- [ ] Expand `docs/architecture.md` with components and search sequence.
- [ ] Document cancellation, ownership, and threading contracts.
- [x] Document schema, migrations, and recovery in `docs/storage.md`.
- [ ] Document format, generations, and invalidation in `docs/indexing.md`.
- [x] Document branch/worktree/overlay in `docs/git-awareness.md`.
- [ ] Document metrics and methodology in `docs/performance.md`.
- [ ] Document states, shortcuts, and accessibility in `docs/ui.md`.
- [ ] Keep per-platform build instructions tested in CI.
- [ ] Add troubleshooting guide for Qt, vcpkg, toolchains, and runtime DLLs.
- [ ] Record important architectural decisions as ADRs.
- [ ] Create contributor documentation and test architecture documentation.
- [ ] Provide documentation in Portuguese and English.
- [ ] Remove unnecessary line breaks in the middle of paragraphs.

## Advanced evolutions (P3)

- [ ] Structural and symbol search with language-aware ranking.
- [ ] Compound queries with boolean operators and persistent filters.
- [ ] Search historical commit content as opt-in behavior.
- [ ] Compare results between branches/worktrees.
- [ ] Shareable indexes only after a safe portability and privacy model exists.
- [ ] Specialized preview for relevant formats without compromising security.
- [ ] Stable local automation API.
- [ ] Evaluate SIMD acceleration and memory mapping only with benchmarks and portable fallback.

## Gates for considering version 1.0 professional

- [ ] Correctness covered for all documented search semantics.
- [ ] Reliable and cancellable direct, indexed, and hybrid search.
- [ ] Git-aware incremental index validated with branches, detached HEAD, and worktrees.
- [ ] No heavy operation on the UI thread.
- [ ] Configurable and tested memory, disk, and queue budgets.
- [ ] Reproducible builds and green CI on supported platforms.
- [ ] Benchmarks without critical regressions and published targets.
- [ ] Accessibility, i18n, installation, update, and uninstallation validated.
- [ ] Complete documentation, licenses, security, and release process.
- [ ] Long-running tests on large real repositories without lost results or index corruption.
