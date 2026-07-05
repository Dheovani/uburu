# Storage

SQLite stores catalogs for repositories, worktrees, paths, and index generations, as well as preferences, search history, saved searches, and operational metrics. Indexed content is referenced by hash; paths point to documents and do not constitute their identity.

The backend uses short transactions, WAL, versioned migrations, and prepared statements. Writes of a new generation are atomic. FTS5 may accelerate parts of the query in the future, but it must remain behind a replaceable interface and must not be the only representation of the index.

## Initial backend

`SQLiteStorageService` is the first concrete `StorageService` backend. It keeps `sqlite3.h` out of the header, opens the connection through RAII, closes it in the destructor, and fails explicitly when used before `initialize()`. The implementation uses prepared statements for reads and writes, plus short transactions for operations that update more than one table.

During initialization, the backend configures and exposes through `pragmaSnapshot()`:

- `PRAGMA foreign_keys = ON`;
- `PRAGMA journal_mode = WAL`;
- `PRAGMA synchronous = NORMAL`;
- `sqlite3_busy_timeout` with an initial 5-second timeout;
- `PRAGMA user_version = 7`;
- the `schema_migrations` table to record applied versions.

These pragmas are part of the storage operational contract. Automated tests verify that the database opens with foreign keys enabled, WAL active, busy timeout configured, and integrity reported as valid.

## Database location and migration

`defaultStorageDatabasePath()` chooses a default location per platform:

- Windows: `%LOCALAPPDATA%/uburu/uburu.db`, falling back to `%APPDATA%`, `%USERPROFILE%`, and the temporary directory;
- macOS: `$HOME/Library/Application Support/uburu/uburu.db`;
- Linux and other Unix systems: `$XDG_DATA_HOME/uburu/uburu.db` or `$HOME/.local/share/uburu/uburu.db`.

`migrateStorageDatabase()` copies the source database to the destination and preserves `-wal` and `-shm` sidecars when they exist. Migration is a safe copy: it creates the destination directory, overwrites the destination when requested by the application, and does not delete the source database. Removing or archiving the old database belongs to the application/configuration layer, not to the low-level helper.

## Schema

The schema evolves through idempotent migrations:

1. `repositories`, `worktrees`, `generations`, `documents`, `files`, and `overlays`;
2. explicit algorithms for content hash and Git blob hash;
3. persistent document identity by `(content_hash_algorithm, content_hash)`;
4. product metadata: `preferences`, `search_history`, `saved_searches`, and `indexing_metrics`;
5. explicit internal indexed-document format version in `documents` and `files`;
6. persisted `mtime` in `files` for conservative incremental reuse by catalog;
7. normalized indexed text in `documents.indexed_text` for content search without rereading files.

Main tables:

- `schema_migrations`: migration versions already applied;
- `repositories`: logical Git repositories;
- `worktrees`: physical worktrees, including `locked`, `prunable`, and lock reason states;
- `generations`: index generations separated by repository, worktree, HEAD, and branch;
- `documents`: documents addressed by content hash algorithm and value;
- `files`: association between relative path in the worktree and indexed document;
- `overlays`: persistent contract for working tree overlay over versioned content;
- `preferences`: global and per-repository preferences;
- `search_history`: recent search history with limited retention;
- `saved_searches`: user-named searches;
- `indexing_metrics`: recent indexing metrics with limited retention.

Even at this stage, path and content are not the same identity: `files` points to `documents` by `content_hash`. The same document can be reused by multiple paths, worktrees, or branches once the indexer starts filling real generations.

`documents` and `files` store the algorithm together with the hash value. For Uburu-owned content, the domain uses `ContentHashAlgorithm`; for Git blobs, it uses `GitObjectHashAlgorithm`. The primary key of `documents` includes algorithm and hash value, avoiding semantic collisions between different algorithms that produce the same textual representation.

`documents` and `files` also store `format_version`. This version describes the internal indexed document format, not the SQLite schema version. It allows future Uburu versions to migrate, reuse, or discard cached documents safely without relying only on table structure.

`documents.indexed_text` stores the first persisted textual representation of content. The field is addressed by the same document hash, so multiple paths, branches, or worktrees can reuse the same indexed text when they point to identical content. The initial representation is normalized line text and does not replace future tokenization, FTS, or compression.

`files` stores `mtime` as native `std::filesystem::file_time_type` ticks. This value is not treated as a user-facing date or Unix timestamp; it exists for local round-trip and incremental comparison between the persisted catalog and the next `FileEntry` observed by scanning.

## Publishing generations

`StorageService::publishGeneration()` publishes a complete worktree view in a `BEGIN IMMEDIATE` transaction. The operation:

1. creates a row in `generations` that is not yet published;
2. removes the previous `files` view for the worktree;
3. inserts or reuses documents by `content_hash`;
4. recreates the generation paths in `files`;
5. marks the generation as published;
6. commits the transaction.

If any document belongs to another repository or worktree, or if any write fails, the transaction is rolled back. The index should therefore never observe half of the new generation and half of the previous one. Tests cover atomic publication, rollback of invalid generations, and consistency between reader and writer connections.

## Recovery, integrity, and rebuild

Migrations run inside explicit transactions. If a migration fails, the transaction is rolled back and the database must not advance `user_version`. During `initialize()`, the backend also removes `generations` records left with `published = 0`, representing interrupted publications before they became visible.

`StorageService::recoverIncompleteGenerations()` exposes the same cleanup for indexer-controlled calls. The method removes only unpublished generation metadata; it does not remove documents or the previously published view.

`StorageService::validateIntegrity()` runs `PRAGMA integrity_check` and returns an explicit report. When the index structure must be discarded without deleting product metadata, `StorageService::rebuildIndexCatalog()` removes `overlays`, `files`, `documents`, and `generations` in a transaction. This rebuild preserves repositories, worktrees, preferences, history, saved searches, and metrics.

`StorageService::collectOrphanDocuments()` removes documents no longer referenced by any path in `files`, using the composite identity `(content_hash_algorithm, content_hash)`. This collection is separate from generation publication to allow future retention, disk-budget, and diagnostics policies before reusable cache is deleted.

`StorageService::enforceDocumentBudget()` applies the first disk-budget policy for the index. The policy is conservative: it computes total bytes stored in `documents` and removes only orphan documents, starting with the oldest, until the total fits the requested limit. Documents still referenced by `files` are never removed by this routine. If the budget remains exceeded because all remaining content is still live, the method returns `budgetExceeded = true` so an application layer can decide between reconfiguring the limit, requesting explicit cleanup, or rebuilding the index.

## Preferences, history, and metrics

Preferences use a textual key and a scope:

- empty scope: global preference;
- `RepositoryId`: repository-specific preference.

Search history and indexing metrics receive a retention limit at write time. The backend inserts the new record and removes the oldest records that exceed the budget, avoiding unbounded growth. Saved searches are identified by name and can be updated without duplicating records.

These tables are deliberately simple at this stage. Semantic validation of keys, preference types, history privacy, and export/import policies belong to future configuration and UX milestones.

## Initial FTS5 evaluation

Milestone 5 introduced `uburu-storage-fts5-benchmark`, a developer benchmark disabled from the default build. It creates a deterministic SQLite dataset, compares a simple textual query through `LIKE` with an equivalent FTS5 query, and validates that both return the same count.

Initial local result on the `core-windows-msvc-debug` preset:

- documents: 20,000;
- matching documents: 2,858;
- repetitions per strategy: 30;
- `LIKE`: 228,924 us;
- FTS5: 9,993 us.

This measurement justifies keeping FTS5 as a strong candidate for accelerating indexed textual queries, but does not change the storage contract: the persistent index remains content-addressed and Git-aware, and FTS5 must be treated as a replaceable auxiliary backend/structure.

## Scope of this stage

This stage persists and retrieves `RepositoryInfo`, `WorktreeInfo`, and `IndexDocument`, including logical removal by path, atomic generation publication, interrupted-publication recovery, orphan document collection, measured pragmas, integrity reports, safe catalog rebuild, preferences, history, saved searches, recent metrics, basic database location/migration, and an initial FTS5 benchmark evaluation.
