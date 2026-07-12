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
- `4`: search was cancelled.

## Scope

The first implementation intentionally supports direct search only. Indexed search, index status, rebuild commands, streaming cancellation from operating-system signals, and richer diagnostics are planned for the same CLI layer without changing the core search engine.
