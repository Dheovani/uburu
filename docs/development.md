# Development

This document records the repository's operational rules. It complements `AGENTS.md`, which defines the technical vision and implementation standards.

## Main branch

The project's main branch is `main`.

Larger work should happen in short, descriptive branches, preferably with a prefix for the affected area:

- `build/...`
- `search/...`
- `ui/...`
- `docs/...`
- `git/...`

## Commit policy

Use small, reviewable commits oriented around a clear intent. The message should follow this format:

```txt
type(scope): short imperative summary
```

Examples:

```txt
feat(search): add deterministic direct search ordering
build: add reproducible CMake presets
docs: document direct search semantics
```

Recommended types:

- `feat`: new behavior.
- `fix`: bug fix.
- `test`: tests.
- `docs`: documentation.
- `build`: CMake, vcpkg, scripts, CI, and tooling.
- `refactor`: internal change without new behavior.
- `chore`: maintenance without direct functional impact.

## Before committing

For the validated Windows/MSVC flow:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check
```

When a change touches C++ code, run `format` before `format-check`:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format
```

To validate the core with the same initial CI bar:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-werror-debug
```

On Linux, also run `core-linux-sanitize-debug` before sensitive changes in parsing, regex, filesystem, concurrency, or storage.

The test-suite structure and fixture policy are documented in [testing.md](testing.md). Use that document when deciding whether a new check belongs in unit tests, integration tests, desktop tests, fuzzers, or benchmarks.

## Coverage

Coverage is optional and intentionally isolated from normal developer builds. Use the Linux core coverage preset when validating broad test-suite changes:

```bash
cmake --preset core-linux-coverage-debug
cmake --build --preset core-linux-coverage-debug
cmake --build --preset coverage
```

The `coverage` target runs the test suite and generates per-module reports under `build/core-linux-coverage-debug/coverage`, currently split between `src/core` and `src/app`. CI publishes this directory as the `linux-coverage-report` artifact.

The initial thresholds are intentionally modest and behavior-oriented: `src/core` and `src/app` both require 30% line coverage and 15% branch coverage. They are not a vanity score; their job is to prevent accidental collapse while the regression suite grows. Raise them only when a critical behavior is covered by deterministic tests and the new value is sustainable in CI.

## Fuzzing

Fuzzing is optional and isolated in a Clang/libFuzzer preset:

```bash
cmake --preset core-linux-fuzz-debug
cmake --build --preset core-linux-fuzz-debug
cmake --build --preset fuzz-smoke
```

The smoke target currently exercises literal matching, `.gitignore` parsing and matching, text-file reading/encoding, and path normalization. CI runs a short smoke pass to catch obvious crashes. Longer exploratory runs should be launched manually from the individual fuzzer executables in `build/core-linux-fuzz-debug/tests/fuzzers`.

## What not to commit

Do not commit:

- `build/`, `build-*`, `dist/`, or `out/` directories;
- `.env`;
- `CMakeUserPresets.json`;
- local databases, logs, or generated artifacts;
- vendored dependencies without an explicit decision.

## Relationship with TODO

`TODO.md` is the project's operational plan. When a change completes a TODO item, mark that item in the same change and validate according to the criteria described in the file itself.
