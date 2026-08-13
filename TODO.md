# TODO — Uburu

This document now tracks only future work. Completed work for the stable 1.0 release is summarized in `CHANGELOG.md`, `docs/releases/v1.0.0.md`, and the project documentation.

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

Uburu `1.0.0` is the first stable Windows and Linux release. It includes the Qt/QML desktop app, CLI, direct, indexed, and hybrid search, Git-aware incremental indexing, SQLite storage, bounded document extractors, tests, benchmarks, documentation, Windows installer packaging, Linux AppImage packaging, checksums, SBOM, and release notes.

Future work below should improve this baseline without reopening completed milestones as checklist noise.

## Before releasing a commercial version

- [ ] Sign Windows release artifacts if a real code-signing certificate becomes available.
- [ ] Review third-party licenses again before any commercial or broader public distribution.

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
