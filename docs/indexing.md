# Indexing

The persistent catalog is split between mutable worktree metadata and content-addressed documents.

A logical entry contains `repositoryId`, `worktreeId`, relative path, size, mtime, content hash, optional Git blob hash, and local status. The content document can be reused when the same hash appears in another branch or worktree.

## Versioned internal format

Every indexed document has `formatVersion`. The current version is `1` and represents a content-addressed document with SHA-256 content hash, optional Git blob metadata, and working tree overlay support. This version is persisted in SQLite together with the document and the path that points to it.

`core/index` owns the interpretation of this format. `core/storage` only persists the version so future upgrades can decide whether a document can be reused, migrated, or discarded. The document format is separate from the SQLite schema version: a schema migration can exist without changing indexed-document semantics, and a new document version can require reindexing even when the schema remains compatible.

## Incremental update

1. Capture HEAD, branch, and Git index state.
2. Consume watcher events and reconcile them with a periodic scan.
3. Compute hashes only when size/mtime or Git state indicate a change.
4. Reuse documents by blob hash or content hash.
5. Apply the working tree overlay for added, modified, and deleted files.
6. Publish a new index generation atomically.

A branch switch invalidates the visible catalog, not the content-addressed storage. Backpressure and a memory budget must limit parsing and result queues.

The first incremental base compares the persisted catalog entry with the current `FileEntry`. When the path in the same worktree is still clean, not deleted, has the same size, the same `mtime`, and a valid persisted hash, the indexer reuses the document identity without rereading the file. This optimization is deliberately conservative: modified files, deleted files, unknown statuses, or files later marked by Git overlay return to the revalidation/reindexing path.

## Document reuse

Storage exposes separate queries for reuse by `contentHash` and by `gitBlobHash`. These queries return only the reusable document identity, not a full file entry, because the same content can appear in multiple paths, branches, or worktrees.

The file catalog remains responsible for linking a document identity to the visible path in the current worktree. This separation avoids treating `path` as content identity and prepares the incremental indexer to prioritize Git blob reuse before rereading working tree files.

When the indexer receives reliable Git metadata for a clean file, it first queries reuse by blob hash. If the blob is already in storage, a new catalog entry is created pointing to the same content-addressed document, without opening or hashing the working tree file. Locally modified files, added files, or files without a reliable blob continue through the content-hash path until the full Git overlay is applied.

## Working tree overlay

Git state participates in the incremental decision before any size/mtime reuse. Clean entries can reuse the persisted catalog or a document known by blob hash. Locally modified entries are always revalidated by reading the working tree, even when size and `mtime` still match the previous catalog, because Git has already reported that the user-visible content changed.

Deleted entries publish a tombstone in the new generation when a previous document existed for the path. This tombstone keeps the previous content identity only as historical reference, but marks the file as `deleted` so future indexed searches do not return stale results from the versioned generation. Deleted files without previous entries are counted as removed, but do not create a new document.

`buildOverlayIndexCandidates()` is the pure bridge between the scanner and the Git overlay: it receives the `FileEntry` values found in the working tree and the `GitOverlayEntry` values, returning indexing candidates with Git status, reusable blob, and tombstones for hidden paths. Renames preserve the current path as a working-tree candidate and generate a tombstone for the previous path, preventing indexed search from keeping both paths visible after reconciliation.

`IndexService::update(worktree, files, overlay)` uses this translation before publishing the incremental generation. The future orchestrator can therefore combine filesystem scanning with `GitService::workingTreeOverlay()` without knowing tombstone, rename, or Git-status reuse-invalidation details.

`DefaultIndexingService` is the first application orchestrator for this flow: it scans the worktree root, reads `GitService::workingTreeOverlay()`, and only then calls `IndexService`. If the Git overlay cannot be read, the update fails and does not publish a new generation, avoiding replacement of a Git-aware index with a blind filesystem view.

Indexed search queries visible documents by worktree root and supports path metadata for `fileName` and `contentAndFileName`. It ignores tombstones (`deleted = true`) and therefore does not return the old path of a locally deleted or renamed file. The first persisted-content support stores normalized text in `documents.indexed_text`, addressed by content hash. This enables real `content` queries without rereading the working tree and without fabricating results from hashes. This representation does not replace a more sophisticated textual backend: tokenization, FTS, compression, ranking, and global highlights remain future evolutions.

`IndexService::staleness()` compares the last published generation for the worktree root with the current `HEAD` and branch. The resulting state differentiates missing, fresh, and stale indexes, and indicates whether the change came from `HEAD`, branch, or both. The UI can use this contract to display indexing status without querying SQLite directly.

Watcher events are initially reconciled in batches by `DefaultIndexingService`. An empty batch does no work; any batch with an event, overflow, or rescan marker triggers a single transactional index update. This policy is conservative and prioritizes correctness: per-file partial reconciliation can replace the full rescan in the future without changing the service's external contract.

## Content hash

The initial content hash algorithm is SHA-256. The choice favors correctness, stability, and a very low practical collision probability for local deduplication, even when the same content appears in different branches, worktrees, or paths.

Hashing is streamed for files, with cooperative cancellation between chunks, avoiding loading whole large files into memory. The `uburu-content-hash-benchmark` benchmark measures throughput on a deterministic synthetic dataset and should be used to compare compilers, flags, and platforms before changing the algorithm or adding an accelerated implementation.

## Initial indexing

`PersistentIndexService` implements the first persistent indexing path. It receives a list of `FileEntry` values, computes streaming SHA-256 for textual files, reuses documents already known by content hash, and publishes a new generation to storage.

Milestone 12 introduces `core/document` as the extraction boundary that indexers should use before deciding how to store content. `PlainTextExtractor` is the initial adapter over the existing text reader. `HtmlDocumentExtractor` handles `.html`, `.htm`, and `.xhtml` by indexing visible text, decoding common entities, and excluding scripts, styles, and comments from indexed content. `SubtitleDocumentExtractor` handles `.srt` and `.vtt` by indexing cue text with timestamp locations while ignoring WebVTT headers and note blocks. `RtfDocumentExtractor` handles `.rtf` by extracting visible text, decoding common text escapes, ignoring embedded objects and image destinations, and enforcing group-depth, control-word, and binary-payload limits. Rich extractors for PDF, OOXML, OpenDocument, and message formats must report explicit extraction statuses instead of collapsing all unsupported or unsafe files into generic read failures. The catalog continues to represent the visible file path even when extracted text is unavailable, so file-name search remains correct independently from content extraction. Unsupported formats, binary files, and temporary extraction limitations are published as name-only indexed documents with `indexed_text = NULL` while still incrementing their skip counters.

Indexing maps document extraction outcomes through `DocumentContentAvailability`. Unsupported formats, binary files, safety-limited files, and protected documents remain name-only searchable. Parser/open/read failures are counted as indexing failures because the extractor selected the file and could not safely interpret it. Cancellation remains a cooperative run state rather than a document error.

Progress is reported by callback, with total, processed, indexed, reused, removed, skipped, and failed counters. Skipped files are classified separately from real failures: unsupported document formats, detected binaries, size/filter skips, and temporary parser limitations must not be shown as indexing failures. Cancellation is cooperative: if the token is signaled before or during hashing, the operation returns `cancelled` and does not publish a partial generation. Isolated open/read failures are counted as failures and do not prevent publication of the remaining valid documents.

`IndexUpdateSummary::extractorMetrics` and `IndexUpdateProgress::extractorMetrics` aggregate per-extractor indexing cost and outcomes. Extractor names include real extractors such as `plain-text`, `html`, `rtf`, and `subtitle`, plus name-only buckets such as `unsupported-format` and `binary-sample`. Each metric records processed files, source bytes, extraction time, unsupported skips, binary skips, safety-limited skips, protected-document skips, parser failures, and indexed extracted-text bytes. These counters are intended for diagnostics and future UI reporting; they do not change search semantics.
