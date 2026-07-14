# Changelog

All notable changes to Uburu are documented in this file.

The project follows semantic versioning. Versions before `1.0.0` are preview releases and may still change internal storage, index, CLI, and settings contracts when documented in the release notes.

## [Unreleased]

- Continue validating large real-world repositories and performance baselines before the first stable release.
- Continue hardening search/index memory budgets, release signing, and optional future distribution targets.

## [0.1.0] - 2026-07-13

First Windows and Linux preview release of Uburu.

### Added

- Established a Qt-independent C++23 core with a separated application layer, desktop UI, CLI entry point, CMake/vcpkg build presets, CI, formatting checks, static analysis, sanitizers, licensing, contribution, security, and release documentation.
- Implemented direct search with literal, case-sensitive, case-insensitive, whole-word, code-identifier, regex/PCRE2, file-name, extension, glob, directory, size, hidden-file, binary-file, and `.gitignore`-aware behavior.
- Added robust text handling for UTF-8, UTF-16 LE/BE, Latin-1 fallback, invalid encoding policy, CRLF/LF/CR line endings, long lines, chunked large-file reads, context extraction, highlighting, and progressive cancellable results.
- Built filesystem traversal with normalized paths, include/exclude scopes, hidden/ignored/binary metrics, symlink and cycle policies, native watcher interfaces, worker pools, bounded queues, backpressure, and cooperative cancellation.
- Added Git-aware repository and worktree modeling through libgit2 with branch, HEAD, detached HEAD, worktree, status, overlay, blob-hash, branch-switch, submodule, and fallback-adapter support.
- Implemented SQLite-backed storage with migrations, WAL, prepared statements, repositories, worktrees, generations, files, documents, overlays, preferences, search history, metrics, recovery, retention, and safe rebuild paths.
- Added persistent and incremental indexing with content-hash and Git-blob reuse, generation publishing, working-tree overlays, staleness detection, indexed search, hybrid search refinement, disk budgets, and pause/resume/manual reindex controls.
- Introduced application-level search services, DTOs, progressive event channels, adaptive result batching, search IDs, structured logging, metrics, diagnostics, tracing, and privacy-safe log handling.
- Redesigned the Qt/QML desktop experience with responsive layout, search scope selector, recent/favorite paths, filters, debounced search, live metrics, indexing progress, virtualized results, preview highlighting, file action menu, command palette, themes, translations, accessibility notes, and persistent UI state.
- Added document extraction for PDF, DOCX, XLSX, PPTX, OpenDocument formats, RTF, HTML/XHTML, SRT, VTT, and plain text, while keeping name-only search available for unsupported or unsafe formats.
- Added archive safety checks for ZIP-backed formats, decompression limits, parser limits, hostile-file handling, extractor metrics, and fuzzing for document and parser boundaries.
- Added a thin `uburu` CLI over the same core services with human output, JSON Lines, stable exit codes, streaming, index commands, and `Ctrl+C` cancellation.
- Added extensibility foundations for symbol parsers, language parser registration, backend contracts, watcher/index adapters, and API/ABI stability documentation.
- Added unit, integration, desktop, regression, coverage, fuzzing, benchmark, and performance-target infrastructure with deterministic datasets and documented methodology.
- Added Windows MSVC packaging, Inno Setup installer generation, Linux AppImage packaging, checksums, SBOM, license report, release notes, validation records, and a preview/stable channel policy.
- Completed professional documentation, ADRs, user guides in English and Portuguese, troubleshooting, build/release guides, privacy/threat-model notes, testing architecture, storage/indexing/Git/search semantics, and supported-format documentation.

### Known limitations

- Windows release artifacts are not code-signed yet.
- Large-repository performance still needs broader validation and tuning before a stable `1.0.0` release.
- macOS is not a supported release target for this preview.
- Legacy binary Office formats, email containers, OCR/image-content search, Flatpak, and automatic update transport remain future evaluation items.
