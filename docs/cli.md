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

The CLI supports direct search immediately and can opt into indexed or hybrid search with `--strategy indexed` or `--strategy hybrid`.

## Index commands

```sh
uburu index-status <root> [options]
uburu index-rebuild <root> [options]
```

`index-status` reports whether the current index generation is missing, fresh, or stale for the selected root.

`index-rebuild` scans the selected root and publishes a new persistent index generation using a filesystem worktree identity. Git-aware CLI indexing is intentionally deferred until it can be enabled without risking command-line hangs or surprising latency.

Both commands support:

- `--format human|jsonl`
- `--database PATH`
- `--types txt,md,docx`
- `--no-gitignore`
- `--hidden`
- `--binary`
- `--no-subdirectories`

## Cancellation

Long-running `search` and `index-rebuild` commands handle `Ctrl+C` as cooperative cancellation. The CLI forwards the cancellation request to the same `std::stop_token` path used by the core engine, so partial work can stop without corrupting index state. Cancelled commands exit with code `4`.

Search results are streamed synchronously to standard output. This is intentional backpressure: if the terminal, pipe, or parent process cannot keep up or closes the stream, the CLI stops requesting more results instead of accumulating an unbounded output queue in memory.

Richer diagnostics are planned for the same CLI layer without changing the core search engine.
