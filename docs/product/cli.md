# CLI

The Uburu CLI is a thin command-line application over the same application and core services used by the desktop UI. It exists to make the search engine easier to test, automate, benchmark, and diagnose without duplicating search logic.

The executable command is `uburu`.

## Search

```sh
uburu search <root> <expression> [options]
```

Example:

```sh
uburu search C:/Users/dheov/Documents truth --types txt,md --format jsonl
```

Supported options:

- `--format human|jsonl`: chooses human-readable output or newline-delimited JSON.
- `--strategy direct|indexed|hybrid`: chooses the search strategy. The default is direct search.
- `--database PATH`: overrides the CLI index database. The default CLI database is `.uburu-cli/uburu-cli-v1.db` in the current working directory and is separate from the desktop application database.
- `--types txt,cpp,md`: restricts file extensions.
- `--max-size-mib N`: sets the maximum file size in MiB.
- `--memory-budget-mib N`: limits approximate retained search-result memory and `index-rebuild` working memory; `0` keeps the budget unlimited.
- `--threads auto|N`: selects direct-search workers. `auto` avoids worker-queue overhead for small files and uses the bounded hardware-aware pool for larger files; an explicit value must be between 1 and 256.
- `--summary-only`: suppresses individual result records while preserving the complete search and aggregate summary, allowing privacy-safe validation and performance collection without retaining paths or matching text.
- `--regex`: treats the expression as a PCRE2 regex when the backend is available.
- `--case-sensitive`: enables case-sensitive matching.
- `--whole-word`: matches whole words.
- `--no-gitignore`: disables `.gitignore` handling.
- `--hidden`: includes hidden files.
- `--binary`: includes binary files.
- `--no-subdirectories`: searches only the selected root.

## Exit codes

Exit codes are part of the CLI contract:

- `0`: command completed successfully and, for search, at least one match was found.
- `1`: search completed successfully but found no matches.
- `2`: command-line usage error.
- `3`: search failed before completion.
- `4`: search or index rebuild was cancelled.

## Scope

The CLI supports direct search immediately and can opt into indexed or hybrid search with `--strategy indexed` or `--strategy hybrid`. Hybrid output is emitted after direct validation so its append-only stream cannot leave stale indexed matches in the final result set.

## Index commands

```sh
uburu index-status <root> [options]
uburu index-rebuild <root> [options]
```

`index-status` reports whether the current index generation is missing, fresh, or stale for the selected root.

`index-rebuild` scans the selected root and publishes a new persistent index generation using a filesystem worktree identity. Git-aware CLI indexing is intentionally deferred until it can be enabled without risking command-line hangs or surprising latency.

When `index-rebuild` reaches its working-memory budget, it exits with code `3`, leaves the previous complete generation untouched, and reports `memoryLimitReached` plus `workingMemoryPeakBytes` in human and JSON Lines summaries. This state is distinct from `Ctrl+C`, skipped unsupported files, and extractor safety limits.

Both commands support:

- `--format human|jsonl`
- `--database PATH`
- `--memory-budget-mib N`
- `--types txt,md,docx`
- `--no-gitignore`
- `--hidden`
- `--binary`
- `--no-subdirectories`

## Cancellation

Long-running `search`, `index-rebuild`, and `document-inspect` commands handle `Ctrl+C` as cooperative cancellation. The CLI forwards the cancellation request to the same `std::stop_token` path used by the core engine, so partial work can stop without corrupting index state. Cancelled commands exit with code `4`.

For repeatable automation and release validation, these commands also accept `--cancel-after-ms N`. The deadline requests cancellation through the same cooperative token used by `Ctrl+C`; it does not terminate the process forcibly. Values range from 1 millisecond to 24 hours.

Search results are streamed synchronously to standard output. This is intentional backpressure: if the terminal, pipe, or parent process cannot keep up or closes the stream, the CLI stops requesting more results instead of accumulating an unbounded output queue in memory.

Search summaries expose `resultLimitReached`, `memoryLimitReached`, and `resultMemoryBytes` in both human and JSON Lines output. Memory exhaustion is a successful bounded completion rather than cancellation or a parser failure: already emitted results remain valid, and the next result that would exceed the configured budget is not published.

JSON Lines summaries also expose time to first result, total duration, processed files and bytes, throughput, and peak queue occupancy. These aggregate fields contain no search expression, path, or matching content and are the supported input for product-validation records.

Richer diagnostics are planned for the same CLI layer without changing the core search engine.

## Document extraction diagnostics

```sh
uburu document-inspect <root> [options]
```

`document-inspect` scans the selected scope and reports aggregate extractor statuses and specific safety or parser issues. It deliberately omits file paths and extracted text, making its output suitable for privacy-safe validation evidence. Use `--types`, `--max-size-mib`, `--no-subdirectories`, and the other common scope flags to reproduce the document scope used by search or indexing.
