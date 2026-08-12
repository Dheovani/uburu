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

- [ ] Execute the repeatable validation matrix in `docs/validation.md` against larger real-world repositories and user datasets, recording evidence for Windows and Linux.
  - [x] Define the repeatable Windows/Linux validation matrix, dataset profiles, evidence format, and acceptance rules.
  - [x] Automate the Windows core, Werror, format, test, and stable benchmark gates used before manual validation.
  - [x] Automate privacy-safe aggregate evidence collection for direct, indexed, hybrid, and incremental-index validation runs.
  - [x] Record a preliminary Windows cycle against the Uburu repository to validate the evidence workflow.
  - [ ] Record a complete Windows validation cycle against representative real-world datasets.
  - [ ] Record a complete Linux validation cycle against representative real-world datasets.
- [ ] Strengthen direct, indexed, and hybrid search so cancellation, partial failures, stale index entries, and refinement behavior remain reliable under long-running workloads.
  - [x] Implement bounded parallel direct search with cooperative cancellation, backpressure, deterministic publication, and configurable worker count.
  - [x] Make hybrid direct validation authoritative so stale indexed matches are not exposed as final results.
  - [x] Replace quadratic hybrid refinement with ordered lookup and stream authoritative direct results without retaining a second complete result collection.
  - [x] Add automated regression coverage for pre-cancellation, sink cancellation, partial failures, stale indexed entries, and hybrid cache classification.
  - [ ] Exercise cancellation, partial failures, and hybrid convergence against the large real-world validation datasets.
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

- [ ] Improve settings screens for advanced search behavior, default ignored directories, ignored extensions, thread count, memory limits, index location, and result limits.
- [ ] Improve visual explanation for include/exclude scope modifiers without making the main search header noisy.
- [ ] Add richer diagnostics for skipped files, unsupported formats, extractor limits, index state, and performance bottlenecks.
- [ ] Continue accessibility validation for keyboard-only use, screen readers, high contrast, focus order, and high-DPI/fractional scaling.
- [ ] Continue reviewing `pt-BR` and `en-US` translations as visible UI text changes.

## Packaging and distribution evolution

- [ ] Evaluate Flatpak only after the AppImage path is stable and Linux filesystem access behavior is clear.
- [ ] Evaluate macOS packaging only if macOS becomes a supported release target with access to real macOS validation hardware.
- [ ] Add automatic update transport only if the project later needs it; do not treat it as required for the current preview line.
- [ ] Keep release notes, checksums, SBOM, license reports, and validation records updated for every public artifact.
