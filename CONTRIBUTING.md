# Contributing

Thank you for considering contributing to Uburu. The project is still in its foundation phase, so the priority is to preserve architecture, correctness, and reproducibility.

## Recommended flow

1. Open or choose an item from `TODO.md`.
2. Create a short branch from `main`.
3. Keep the change small and reviewable.
4. Update documentation when behavior, architecture, or commands change.
5. Run build, tests, and `format-check` before opening a review.

## Minimum local commands

On the validated Windows/MSVC flow:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check
```

To work without Qt, use the `core-windows-msvc-debug` preset.

## Style

- C++ code must follow the root `.clang-format`.
- New project-owned files must use `kebab-case`, except canonical tool names.
- Do not introduce user-visible text without going through i18n.
- Do not couple search logic to QML.
- Avoid magic numbers; prefer named constants with the smallest suitable scope.
- Prefer direct code, while preserving clarity and semantic equivalence.

## Commits

Use the format documented in `docs/development.md`:

```txt
type(scope): short imperative summary
```

Example:

```txt
build: add core ci quality gates
```
