# Uburu

English | [Português do Brasil](README.pt-BR.md)

Uburu is a desktop application for advanced search across files and software repositories. The current base provides direct and progressive literal search, a non-blocking Qt Quick UI, and contracts for future persistent, Git-aware indexing.

The complete evolution plan and the criteria for version 1.0 are in [TODO.md](TODO.md). Branch, commit, and local validation rules are in [docs/development.md](docs/development.md).

## Dependencies

- CMake 3.25+
- compiler with C++23 support
- Visual Studio 2026/MSVC on Windows; Ninja remains supported for specific flows
- Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Concurrent`, `LinguistTools`)
- Catch2 3 for tests
- SQLite, PCRE2, and libgit2 are detected by the core at this stage

The `vcpkg.json` manifest provides non-Qt dependencies. Qt may be installed through the system's preferred package manager or through a vcpkg overlay feature.

## Initial version policy

- Minimum CMake: 3.25, because of `CMakePresets.json` and modern build features.
- C++: C++23 without compiler extensions.
- Minimum Qt: 6.5.
- Validated Windows/MSVC: Visual Studio 18 2026 with Qt 6.11.1 `msvc2022_64`.
- vcpkg: required for non-Qt dependencies; the baseline is pinned in `vcpkg.json`.

## Recommended preset build

Configure these environment variables:

- `VCPKG_ROOT`: vcpkg root.
- `QT_ROOT`: Qt prefix used by CMake, for example `C:\Qt\6.11.1\msvc2022_64`.

Use `.env.example` as a reference to create a local `.env`. The `.env` file is ignored by Git. PowerShell scripts in `scripts/` load `.env` automatically; CMake presets still read variables from the process environment, so define them in the terminal before running `cmake --preset ...`.

On Windows with Qt/MSVC:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
.\scripts\run-windows-msvc-desktop.ps1
.\scripts\deploy-windows-msvc-desktop.ps1
```

To work only on the core without a Qt installation:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-debug
```

On Windows, the main local flow uses MSVC and Qt.

## Manual Windows build with PowerShell

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Confirm the vcpkg configuration with `Write-Output $env:VCPKG_ROOT`. In PowerShell, always use `$env:VCPKG_ROOT`; `$VCPKG_ROOT` belongs to POSIX shells.

## Linux or macOS build

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The local `local-windows-msvc-debug` preset uses the Visual Studio generator and does not require a Developer Prompt.

## Formatting

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command tidy
```

## Quality and CI

Initial core gates use Qt-free presets:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-werror-debug
```

On Linux, the equivalent presets are `core-linux-werror-debug` and `core-linux-sanitize-debug`. The workflow in `.github/workflows/ci.yml` runs configure, build, tests, sanitizers, and `format-check` for the core.

See also:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [SECURITY.md](SECURITY.md)
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- [docs/licenses.md](docs/licenses.md)

## Current state

Literal search reads files line by line and respects size limit, extension filtering, hidden files, cancellation, and result limit. Regex, `.gitignore`, complete encoding/binary detection, SQLite storage, indexing, and the libgit2 backend are explicitly reserved by contracts and documentation; the UI informs the user when regex is not available yet.
