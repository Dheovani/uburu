# Build

The preferred flow uses `CMakePresets.json`. It avoids versioned absolute paths and lets each machine provide its own directories through environment variables.

## Environment variables

- `VCPKG_ROOT`: vcpkg root.
- `QT_ROOT`: Qt prefix used by CMake. Windows/MSVC example: `C:\Qt\6.11.1\msvc2022_64`.
- `NINJA_ROOT`: directory containing `ninja.exe`, only for presets that use Ninja.

`CMakeUserPresets.json` is ignored by Git and can be used for personal aliases or paths. `.env.example` lists the expected variables. Copy it to `.env` if you want to record local paths. PowerShell scripts load `.env` automatically; CMake presets still require the variables to be present in the terminal environment.

PowerShell example:

```powershell
$env:VCPKG_ROOT = "C:\Users\dheov\vcpkg"
$env:QT_ROOT = "C:\Qt\6.11.1\msvc2022_64"
```

## Windows with Qt/MSVC

This is the recommended local flow. It uses Visual Studio/MSVC and Qt `msvc2022_64`:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
```

To run the application without installing DLLs system-wide:

```powershell
.\scripts\run-windows-msvc-desktop.ps1
```

To create a local redistributable development folder with `windeployqt` and vcpkg DLLs:

```powershell
.\scripts\deploy-windows-msvc-desktop.ps1
```

The script writes to `dist/windows-msvc-debug` by default.

To create the initial Release portable bundle, ZIP archive, manifest, and SHA-256 checksum:

```powershell
.\scripts\package-windows-msvc-desktop.ps1
```

The script writes to `dist/windows-msvc-release` by default. See [release packaging](release.md) for the manual validation checklist and remaining installer/signing work.

To build the first Windows installer, install Inno Setup 6 and run:

```powershell
.\scripts\build-windows-installer.ps1
```

If Inno Setup is not installed, the script fails before modifying release artifacts and tells you to install `ISCC.exe`.

To prepare the Windows release assets, checksums, and release asset manifest in one step:

```powershell
.\scripts\prepare-windows-release.ps1 -AppVersion v0.1.0
```

## Linux

With Ninja, vcpkg, and Qt available:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

If Qt is installed outside the system's default prefixes, set `QT_ROOT` before configuring.

## Core without Qt

The project supports `UBURU_BUILD_DESKTOP=OFF` to build and test the core without Qt:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-debug
```

On Linux, use `core-linux-debug` for the same flow without Qt.

## Quality presets

The project separates development presets from gate presets. The presets below do not require Qt and are suitable for CI and quick core validation:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-werror-debug
```

On Linux, use `core-linux-werror-debug` to treat warnings as errors. For ASan/UBSan, use `core-linux-sanitize-debug`.

The corresponding CMake options are:

- `UBURU_WARNINGS_AS_ERRORS`: promotes warnings from project-owned targets to errors.
- `UBURU_ENABLE_SANITIZERS`: enables AddressSanitizer and UndefinedBehaviorSanitizer where supported.

Sanitizers are disabled by default so they do not affect normal builds, packaging, or environments where the runtime is unavailable.

## Initial version policy

- Minimum CMake: 3.25.
- C++ standard: C++23, with `CMAKE_CXX_EXTENSIONS=OFF`.
- Minimum Qt: 6.5.
- Validated Windows/MSVC: Visual Studio 18 2026 with Qt 6.11.1 `msvc2022_64`.
- Linux: initial preset with Ninja and the `x64-linux` triplet.
- vcpkg: used for Catch2, SQLite, PCRE2, and libgit2. The baseline is pinned in `vcpkg.json`.

## Manual build

Manual commands remain available when a preset does not fit the environment:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Qt 6.5 or newer must be in `CMAKE_PREFIX_PATH` or `Qt6_DIR`.

## C++ formatting

Every project-owned C++ file in `apps/`, `src/`, and `tests/` must follow the root `.clang-format`. When `clang-format` is available in `PATH` or in a standard Visual Studio installation, CMake creates the optional `format` target.

The build enforces C++23 through `CMAKE_CXX_STANDARD`. In `.clang-format`, `Standard: Latest` is used because the formatter does not provide the `c++23` value; that option enables the newest C++ syntax rules recognized by the installed version.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command tidy
```

You can also format a single file directly:

```powershell
clang-format -i src/core/search/direct-search-engine.cpp
```

The target is not part of the normal build. Vendored dependencies, generated files, and build directories are not formatted.

## Static analysis

The `.clang-tidy` file defines the initial check set. When `clang-tidy` is available, CMake creates the optional `tidy` target:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command tidy
```

This target is diagnostic and is not part of the normal build. Future CI should decide when diagnostics become blocking.

Note: the validated Windows development flow uses MSVC. If `clang-tidy` is incompatible with the local compile database, treat the target as diagnostic and validate build/tests before making the result blocking.

## CI

The `.github/workflows/ci.yml` workflow initially validates the Qt-independent core:

- Windows/MSVC with warnings as errors;
- Linux with warnings as errors;
- Linux with AddressSanitizer and UndefinedBehaviorSanitizer;
- tests through CTest;
- `format-check` without modifying files.

The table below maps the documented build instructions to the current CI coverage:

| Documented flow | CI job | Commands validated |
| --- | --- | --- |
| Windows core/MSVC quality gate | `Windows MSVC core` | `cmake --preset core-windows-msvc-werror-debug`, `cmake --build --preset core-windows-msvc-werror-debug`, `ctest --preset core-windows-msvc-werror-debug`, and `format-check` |
| Linux core quality gate | `Linux core` | `cmake --preset core-linux-werror-debug`, `cmake --build --preset core-linux-werror-debug`, `ctest --preset core-linux-werror-debug`, `format-check`, and `tidy` |
| Linux sanitizer quality gate | `Linux sanitizers` | `cmake --preset core-linux-sanitize-debug`, `cmake --build --preset core-linux-sanitize-debug`, and `ctest --preset core-linux-sanitize-debug` |
| Linux coverage report | `Linux coverage` | `cmake --preset core-linux-coverage-debug`, `cmake --build --preset core-linux-coverage-debug`, and the `coverage` target |
| Linux fuzz smoke | `Linux fuzz smoke` | `cmake --preset core-linux-fuzz-debug`, `cmake --build --preset core-linux-fuzz-debug`, and the `fuzz-smoke` target |

Desktop Qt instructions are intentionally documented as local validation flows for now. The Windows desktop run/deploy scripts depend on a local Qt installation and `windeployqt`, and full desktop/package CI belongs to the packaging milestone. Until then, the CI-supported release confidence comes from the Qt-independent core, CLI, document extraction, indexing, storage, and quality gates above.

## Troubleshooting

If CMake cannot find Qt, verify that `QT_ROOT` points to the Qt CMake prefix, not only to the top-level `C:\Qt` directory. For the validated Windows/MSVC setup, the expected value is similar to `C:\Qt\6.11.1\msvc2022_64`. If you configure manually, pass that path through `CMAKE_PREFIX_PATH` or set `Qt6_DIR` to the directory containing `Qt6Config.cmake`.

If CMake reports `/bin/g++.exe` or another invalid compiler path on Windows, clear the stale build directory or select a Windows/MSVC preset. This usually means VS Code or an old cache injected a MinGW compiler override into a build tree that is now expected to use MSVC.

If vcpkg cannot write to its `buildtrees` directory, check that `VCPKG_ROOT` points to a writable vcpkg checkout and that no antivirus or previous process is locking files under that directory. Reconfiguring a preset may run `vcpkg install`, so the vcpkg tree must be writable even when the Uburu workspace itself is clean.

If the desktop executable starts from the build directory but fails to load Qt or dependency DLLs, use `scripts/run-windows-msvc-desktop.ps1` for development runs or `scripts/deploy-windows-msvc-desktop.ps1` to create a local folder with `windeployqt` output and vcpkg DLLs. Do not copy DLLs into system directories.

If PowerShell refuses to execute project scripts, run them with `powershell -ExecutionPolicy Bypass -File <script>` for the current invocation. Avoid changing the machine-wide execution policy just to build Uburu.

If `format-check` fails after a manual edit, run the `format` target or format only the files you changed. The style checker intentionally accepts the project-specific readability rules from `AGENTS.md`; do not undo those rules just to match an external formatter default.

If Linux sanitizer builds fail while a normal debug build passes, inspect the first sanitizer error before changing tests. AddressSanitizer and UndefinedBehaviorSanitizer often expose lifetime, bounds, or initialization problems that are hidden in the regular build.
