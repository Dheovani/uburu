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

## What not to commit

Do not commit:

- `build/`, `build-*`, `dist/`, or `out/` directories;
- `.env`;
- `CMakeUserPresets.json`;
- local databases, logs, or generated artifacts;
- vendored dependencies without an explicit decision.

## Relationship with TODO

`TODO.md` is the project's operational plan. When a change completes a TODO item, mark that item in the same change and validate according to the criteria described in the file itself.
