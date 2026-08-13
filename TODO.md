# TODO — Uburu

This document now tracks only future work. Completed work for the first preview release is summarized in `CHANGELOG.md`, `docs/releases/v0.1.0.md`, and the project documentation.

## Backlog conventions

- `[ ]` means future work.
- `P0` blocks trust in correctness or data safety.
- `P1` improves the main product experience.
- `P2` professionalizes distribution, maintainability, or integration.
- `P3` is advanced evolution and must not distract from correctness, reliability, or measured performance.
- Do not mark an item as complete unless code, tests, documentation, and applicable metrics are updated.
- Critical core changes require automated tests. Performance changes require before/after benchmarks or a reproducible metric.
- Visible user text must exist in `pt-BR` and `en-US`.

## Current release baseline

Uburu `0.1.0` is the first Windows and Linux preview. It includes the Qt/QML desktop app, CLI, direct search, persistent indexing foundations, Git-aware architecture, SQLite storage, document extractors, tests, benchmarks, documentation, Windows installer packaging, Linux AppImage packaging, checksums, SBOM, and release notes.

Future work below should improve this baseline without reopening completed milestones as checklist noise.

## Before releasing a commercial version

- [ ] Sign Windows release artifacts if a real code-signing certificate becomes available.
- [ ] Review third-party licenses again before any commercial or broader public distribution.

## Before a stable 1.0 release

- [ ] Execute the repeatable validation matrix in `docs/development/validation.md` against larger real-world repositories and user datasets, recording evidence for Windows and Linux.
  - [x] Define the repeatable Windows/Linux validation matrix, dataset profiles, evidence format, and acceptance rules.
  - [x] Automate the Windows core, Werror, format, test, and stable benchmark gates used before manual validation.
  - [x] Automate privacy-safe aggregate evidence collection for direct, indexed, hybrid, and incremental-index validation runs.
  - [x] Make validation evidence require Release builds, record binary/subdirectory/file-size scope, enforce known matches, and distinguish declared partial failures from fatal failures.
  - [x] Record a preliminary Windows cycle against the Uburu repository to validate the evidence workflow.
  - [x] Record a preliminary Windows document-corpus cycle over all 217 PDFs up to 256 MiB and fix the Unicode-path and embedded-NUL regressions it exposed.
  - [x] Add privacy-safe extraction-status counters and align PDF source limits between direct search, scanning, and indexing.
  - [x] Classify and reduce the PDF extraction failures exposed by the Windows document corpus before promoting the run to formal evidence.
  - [x] Validate exact-phrase convergence across direct, indexed, and hybrid search on the 7.35 GB Windows PDF corpus after fixing PDF glyph spacing, ligatures, and indirect font resources.
  - [x] Record formal automated Windows evidence for the 7.35 GB document corpus with a clean worktree and a hashed Release executable.
  - [x] Provide the final three-recording manual validation script in `docs/releases/v1.0.0-manual-validation-script.pt-BR.md`.
  - [x] Fix the desktop debounce/watcher reentrancy exposed by the manual PDF-corpus validation so a pending automatic search cannot replace or discard the manual result.
  - [x] Preserve structured-document section identity so PDF preview opens only the page that produced the selected result.
  - [ ] Record a complete Windows validation cycle against representative real-world datasets.
  - [ ] Record a complete Linux validation cycle against representative real-world datasets.
- [ ] Strengthen direct, indexed, and hybrid search so cancellation, partial failures, stale index entries, and refinement behavior remain reliable under long-running workloads.
  - [x] Implement bounded parallel direct search with cooperative cancellation, backpressure, deterministic publication, and configurable worker count.
  - [x] Make hybrid direct validation authoritative so stale indexed matches are not exposed as final results.
  - [x] Replace quadratic hybrid refinement with ordered lookup and stream authoritative direct results without retaining a second complete result collection.
  - [x] Add automated regression coverage for pre-cancellation, sink cancellation, partial failures, stale indexed entries, and hybrid cache classification.
  - [x] Exercise declared PDF extraction failures and direct/indexed/hybrid convergence against the 7.35 GB Windows document corpus.
  - [x] Enforce bounded `SIGINT`-to-`stop_token` propagation in the CLI test suite.
  - [x] Add a privacy-safe, repeatable real-dataset gate for bounded end-to-end CLI cancellation.
  - [x] Record preliminary Windows cancellation evidence over the 7.35 GB PDF corpus, completing 9 ms after a 250 ms deadline while preserving stable exit code `4`.
  - [ ] Measure interactive cancellation latency against large real-world validation datasets on Windows and Linux.
- [ ] Validate Git-aware incremental indexing with branches, detached HEAD, multiple worktrees, submodules, deleted files, modified files, untracked files, ignored files, and branch switches in larger repositories.
  - [x] Cover the required Git states and reconciliation behavior with disposable automated repositories.
  - [ ] Repeat the complete Git matrix in larger real repositories and record the evidence.
- [x] Define and enforce configurable memory, disk, queue, result, preview, and extractor budgets across direct search, indexing, preview, CLI, and desktop UI.
  - [x] Enforce bounded direct-search queues, configurable worker count, result-count limits, bounded previews, extractor limits, and index disk budget.
  - [x] Connect the persisted global/per-repository memory budget to direct, indexed, and hybrid result production.
  - [x] Audit and expose consistent budget-exhaustion status through the core summary, CLI, and desktop UI.
  - [x] Apply the configured memory budget to indexing working sets and report indexing-memory exhaustion separately from skipped files and extractor safety limits.
- [ ] Publish benchmark baselines and regression targets for representative repository sizes, document-heavy folders, many-small-file datasets, and few-large-file datasets.
  - [x] Add versioned deterministic baselines and median-based regression gates for direct search, queues, indexing, hashing, extraction, batching, and hybrid refinement.
  - [ ] Capture and publish baselines for the representative real-world dataset profiles on Windows and Linux.
- [ ] Revisit end-to-end performance with real user datasets, including startup latency, direct search latency, indexing throughput, preview latency, memory growth, and UI responsiveness before choosing optimization strategies.
  - [x] Measure and tune direct-search worker scaling, queue occupancy, sparse-match workloads, cancellation behavior, and hybrid refinement on deterministic datasets.
  - [x] Record Release-mode search throughput, index throughput/reuse, queue peaks, extraction outcomes, and indexing working memory for the 7.35 GB Windows PDF corpus.
  - [ ] Record end-to-end observations for startup, search, indexing, preview, memory, and UI responsiveness on real datasets.
- [ ] Harden release validation for supported platforms with repeatable clean-machine smoke tests and documented evidence.
  - [x] Document clean-machine artifact, smoke-test, evidence, and release acceptance requirements.
  - [ ] Execute the final Windows installer and Linux AppImage smoke tests for the stable release candidate.

## Search and indexing evolution

- [ ] Improve large-repository performance only with measured bottlenecks and before/after benchmarks.
- [ ] Benchmark and tune the bounded parallel direct-search pipeline on representative Windows and Linux datasets, including worker-count scaling, queue contention, cancellation latency, deterministic publication overhead, and memory use.
- [ ] Evaluate SIMD acceleration and memory mapping only with portable fallbacks and benchmarks.
- [ ] Improve indexed search ranking and hybrid refinement quality without losing deterministic behavior.
- [ ] Add compound queries with boolean operators and persistent filters.
- [ ] Search historical commit content as opt-in behavior.
- [ ] Compare results between branches and worktrees.
- [ ] Explore shareable indexes only after a safe portability, privacy, and invalidation model exists.

## Formats and preview evolution

- [ ] Evaluate support for legacy Microsoft Office formats (`.doc`, `.xls`, `.ppt`) behind an optional extractor or explicit dependency decision, because binary Office parsing is higher risk than OOXML.
- [ ] Evaluate email/message formats (`.eml`, `.msg`) with privacy-safe attachment handling and no automatic traversal into attachments until limits and UX are defined.
- [ ] Evaluate future image-content search with OCR or metadata extraction for formats such as PNG, JPEG, TIFF, and screenshots, keeping it opt-in and benchmarked because it may add heavy dependencies and CPU cost.
- [ ] Improve specialized preview for relevant formats without compromising security or loading large documents unboundedly.

## Extensibility and automation

- [ ] Expand structural and symbol search with language-aware ranking.
- [ ] Evaluate tree-sitter or alternative parsers behind replaceable adapters before committing to a dependency.
- [ ] Stabilize local automation APIs only after internal CLI and service contracts stop changing frequently.
- [ ] Define plugin boundaries only when there is a concrete extension use case and a safe ABI/API compatibility policy.
- [ ] Evolve index-backend and file-watcher contracts with compatibility tests before allowing external implementations.

## Product and UX evolution

- [ ] Improve settings screens for advanced search behavior, default ignored directories, ignored extensions, memory limits, index location, and result limits.
  - [x] Expose the persisted desktop direct-search thread limit, including hardware-aware automatic mode.
- [ ] Improve visual explanation for include/exclude scope modifiers without making the main search header noisy.
- [ ] Add richer diagnostics for skipped files, unsupported formats, extractor limits, index state, and performance bottlenecks.
- [ ] Continue accessibility validation for keyboard-only use, screen readers, high contrast, focus order, and high-DPI/fractional scaling.
- [ ] Continue reviewing `pt-BR` and `en-US` translations as visible UI text changes.

## Packaging and distribution evolution

- [ ] Evaluate Flatpak only after the AppImage path is stable and Linux filesystem access behavior is clear.
- [ ] Evaluate macOS packaging only if macOS becomes a supported release target with access to real macOS validation hardware.
- [ ] Add automatic update transport only if the project later needs it; do not treat it as required for the current preview line.
- [ ] Keep release notes, checksums, SBOM, license reports, and validation records updated for every public artifact.
