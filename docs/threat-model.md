# Threat model

Uburu is a local desktop application that scans user-selected directories, reads files, evaluates user-provided search expressions, stores local indexes, and exports local diagnostics. The main security objective is to keep that work local, bounded, cancellable, and resilient when inputs are hostile, malformed, unexpectedly large, or controlled by another process.

## Assets

- File contents and preview snippets.
- File names, directory names, repository names, branch names, and search expressions.
- Local index database, settings, saved searches, recent paths, logs, and diagnostic reports.
- UI responsiveness and cancellation reliability.
- Correctness of results under filesystem and Git changes.

## Trust boundaries

- User-selected roots are not automatically trusted. A directory can contain hostile files, symlink loops, deep trees, sparse files, huge files, or content that changes while it is being read.
- Search expressions are user input. Regex patterns may be expensive or intentionally pathological.
- Git metadata is local data, but repository contents and hooks are not trusted as executable logic. Uburu should read metadata through `libgit2` or isolated adapters and must not run repository-controlled hooks.
- The SQLite database is local application state. It can be corrupted by crashes, disk failures, manual edits, or older versions of the application.
- Exported diagnostics and settings cross the local privacy boundary once the user chooses to share them.

## Regex risks

Regex search can cause high CPU usage, deep recursion, excessive heap use, or long match times. The current policy is to compile regex once, use PCRE2 limits, distinguish compile failures from resource failures, support cancellation, and keep literal search as the cheaper default. Future changes to regex matching must preserve explicit limits and add benchmarks before relaxing them.

## Hostile files and formats

Files may be binary, malformed, extremely large, sparse, encoded unexpectedly, or modified during reading. Uburu should sample for binary content, apply size and line-length limits, stream text instead of loading whole files, handle read errors as partial failures, and never let one bad file stop a valid search. Future archive, PDF, DOCX, or other document extractors must add decompression and parser limits before becoming default behavior.

## Symlinks and filesystem traversal

Symlinks, junctions, mount points, and recursive directory structures can create cycles or escape the apparent tree. Uburu should not follow directory symlinks by default, must detect cycles when following is enabled, must normalize paths consistently per platform, and must apply include/exclude rules after normalization. Watcher overflow or lost events should trigger reconciliation instead of trusting incomplete notifications.

## Local database risks

The index and settings database may be stale, corrupted, partially migrated, or interrupted mid-publication. Storage must use transactions for migrations and generation publication, keep foreign keys enabled, validate integrity, recover unpublished generations, and provide safe rebuild paths. The database must not be treated as authoritative over the visible working tree when direct validation or overlay reconciliation indicates a mismatch.

## Privacy risks

Paths, search expressions, file names, branch names, and snippets can identify private work. Logs and diagnostics must mask sensitive fields by default. Telemetry is disabled by default and must remain explicit opt-in if it is ever implemented. Import/export features must separate preferences from history-like data so users do not accidentally share private paths or searches.

## Operational rules

- Prefer bounded work: limits, queues, budgets, and cancellation tokens.
- Treat errors as typed, localized, and recoverable whenever possible.
- Keep heavy work off the UI thread.
- Avoid executing repository-provided code or shell commands as part of ordinary scanning.
- Preserve deterministic behavior where it affects result correctness or reproducible tests.
- Require tests and benchmarks for changes that expand parsing, traversal, regex, indexing, or concurrency.
