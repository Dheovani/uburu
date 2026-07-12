# Architecture

## Dependency direction

```text
QML
  -> SearchController
    -> SearchService
      -> SearchEngine
      -> IndexService

SearchEngine -> FileScanner -> TextFileReader -> Text matcher
IndexService -> GitService / StorageService / FileScanner
```

`uburu_core` is a C++23 library with no Qt dependency. The desktop application knows the core through interfaces and converts results into a `QAbstractListModel`; QML never runs searches or accesses files directly.

Shared types live in `src/shared/types`. They model logical repository and worktree identity, relative path, and content identity separately. This prevents turning a path into a document identity.

Important decisions that should remain stable across implementation details are recorded as ADRs in [adrs/README.md](adrs/README.md).

## Search strategies

`SearchService` is the orchestration point between direct search, indexed search, and hybrid search. The choice is not inferred implicitly from the presence of an index: `SearchServiceOptions::strategy` defines whether execution uses only `SearchEngine`, only `IndexService`, or a hybrid combination.

In the hybrid strategy, the service validates the query once, emits fast results from the index, and then runs direct search to confirm and refine the view. Reconciliation uses `search::refineSearchResults()`, which classifies results as confirmed, added, or removed and keeps deterministic ordering. At this stage, the UI layer still receives only progressive results; explicit confirmation/removal events belong to the Milestone 7 event channel.

`SearchService::searchWithEvents()` is the initial contract for that channel. Each execution receives a `SearchRunId`, emits a `started` event, publishes results in batches, and ends with `completed`, `cancelled`, or `failed`. The `runId` lets controllers discard late events from old searches without global state. The service measures `timeToFirstResult` and `totalTime` at the selected strategy level, covering direct, indexed, and hybrid search.

The event channel publishes application-layer DTOs (`SearchEventDto`, `SearchResultDto`, and `SearchSummaryDto`). This prevents the UI from depending on engine details, internal error enums, or persistence formats. Conversion lives in `src/app/dto`, keeping the core reusable by CLI, tests, and future non-Qt interfaces.

Result batch size is controlled by `AdaptiveResultBatcher`. Each execution starts with `SearchExecutionOptions::resultBatchSize`, respects configurable minimum and maximum limits, and adjusts the next batch according to the observed latency when delivering the event to the sink. Cheap deliveries increase the batch to reduce overhead; expensive deliveries reduce it to preserve UI responsiveness.

## Symbols

`core/symbols` defines the language and symbol parsing boundary. It is intentionally separate from `core/index`: the index may consume stable symbol data, but parser backends such as tree-sitter must remain replaceable adapters. See [symbols.md](symbols.md) for the tree-sitter evaluation and backend constraints.

Replaceable backend contracts carry explicit version metadata from `core/contracts`. These contracts remain internal until the stability requirements in [api-stability.md](api-stability.md) are met.

## Concurrency

`SearchController` schedules search on the `QtConcurrent` pool. Results are returned progressively to the UI thread through queued events. The core uses `std::stop_token`, allowing CLI, tests, or other interfaces to use the same cancellation model without Qt.

Future concrete services must be built behind the existing interfaces. Filesystem, Git, or SQLite operations must not migrate into the controller.

## Text reading

`core/text` contains a line-oriented file reader that delivers UTF-8 text to the search engine. The `SearchEngine` does not interpret BOM, UTF-16, Latin-1, binaries, or line endings directly; it consumes `TextLine` and delegates matching to `text-matcher` or `regex-matcher`.

This separation keeps encoding, binary policy, line limits, and preview context outside search logic, preserving the ability to evolve specialized readers without changing the engine's public contract.

## Filesystem and ignore rules

`core/filesystem` contains the recursive scanner and ignore rules. The scanner applies filters before delivering `FileEntry` to the search engine, keeping `SearchEngine` free from directory, hidden-file, glob, and `.gitignore` details.

Path normalization is centralized in `path-normalization`. Relative paths are converted to generic `/` representation, redundant lexical segments are removed, and absolute paths are rejected when an API requires a relative path. Comparison keys follow the platform: on Windows, ASCII path comparisons are case-insensitive; on POSIX platforms, they remain case-sensitive. This keeps filters, globs, deterministic ordering, and ignore rules using the same semantics.

On Windows, normalization does not impose legacy `MAX_PATH` limits and preserves UNC and extended-length prefixes (`\\server\share` and `\\?\...`) as part of the normalized key. Future native backends may still need specific adaptations for Win32 APIs, but the core must not truncate, discard, or compare these paths as ordinary case-sensitive strings.

The scanner detects sparse files when the platform exposes that information. On Windows, detection uses `FILE_ATTRIBUTE_SPARSE_FILE`; on compatible POSIX platforms, it compares allocated blocks with the logical size reported by `stat`. The initial policy is conservative: `FileEntry::sparse` only signals the attribute, and direct search still applies the same size and reading limits as for other files. Sparse-file optimizations or restrictions must be added only with benchmarks and without losing results by default.

Within each directory, the scanner uses deterministic priority: directories are visited before files, smaller files are published before larger files, and the normalized path breaks ties. This improves time to first result in common trees without introducing randomness. Global prioritization between subtrees and probability-based ranking should be handled by a future queue/worker stage.

Symlink directories, junctions, and reparse points are explicit traversal boundaries. By default, the scanner does not enter them. When `SearchOptions::followSymlinks` is enabled, the scanner may traverse them, but records canonical identities of visited directories to avoid cycles. Mount points are treated as normal directories at this stage; a future policy for blocking volume crossing should be added as an explicit option, not as a traversal side effect.

Filesystem watchers use the `FileWatcher` interface, which emits batches of relative create, modify, and remove events. The initial backend is `PollingFileWatcher`: it keeps normalized snapshots and compares size, timestamp, and entry type on each `poll()`. This fallback is portable and simple, but it does not replace efficient native backends. `WindowsFileWatcher`, `LinuxFileWatcher`, and `MacosFileWatcher` implement initial native backends with `ReadDirectoryChangesW`, `inotify`, and `FSEvents`, preserving the same contract. The return value is a `FileChangeBatch`; when a backend detects overflow, lost events, or an incomplete snapshot, it must mark `events_may_be_incomplete` and `requiresRescan`. Callers must treat that signal as invalidation of the incremental sequence and run a reconciliation rescan before trusting new deltas. A factory should choose between native backend and fallback when monitoring is connected to the incremental pipeline.

`FileWatcherFactory` is the internal extension point for selecting watcher backends. It reports backend capabilities and returns either a watcher instance or a typed open error, so callers can fall back from native watchers to polling without hard-coding platform classes.

Concurrent pipelines should use `concurrency::BoundedQueue` to communicate scan, read, match, and publish stages. The queue has fixed capacity, blocks producers when full, wakes consumers on demand, supports explicit closure, and respects `std::stop_token` in blocking operations. This provides the common backpressure mechanism before worker pools are connected to direct search or indexing. Each queue has its own internal mutex; there is no global pipeline mutex. The queue also keeps simple producer and consumer wait metrics so real contention can be exposed when the parallel pipeline starts publishing diagnostics.

`concurrency::WorkerPool` provides the initial configurable worker pool. It normalizes zero workers to one, executes submitted tasks in `std::jthread`, closes the work queue explicitly, and propagates `std::stop_token` to each task. `close()` is ordered draining: it accepts no new tasks, lets already queued tasks finish, and waits for workers. `request_stop()` is cooperative cancellation: it signals workers, closes the queue, and waits for shutdown. The scanner, queue, text reader, regex, and matcher already accept cooperative cancellation; the next step is connecting those pieces into a parallel direct search pipeline without losing deterministic ordering or progressive results.

`.gitignore` rules are loaded per directory. `.gitignore` files in subdirectories add higher-precedence rules for that subtree. The initial implementation covers comments, basename patterns, path patterns, anchored rules, directories, negation, and disabling through `SearchOptions::respectGitignore`.

The scanner also loads `.git/info/exclude` from the searched root and global ignore files passed explicitly through `SearchOptions::globalGitIgnoreFiles`. Automatic discovery of the global Git ignore path stays outside the scanner and should enter through `GitService` or the configuration layer, so Git configuration reading is not mixed with generic filesystem traversal.
