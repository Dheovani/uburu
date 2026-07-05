# Benchmarks

This directory contains reproducible development benchmarks. They stay outside the default build so they do not slow down compilation, tests, or CI.

## Storage FTS5

`uburu-storage-fts5-benchmark` compares a simple textual query in the SQLite catalog using `LIKE` against an equivalent FTS5 query. The goal is to evaluate FTS5 as an auxiliary structure for indexed search without coupling the `StorageService` contract to that backend.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 `
  -Command configure `
  -Preset core-windows-mingw-benchmarks-debug

powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 `
  -Command build `
  -Preset core-windows-mingw-benchmarks-debug

. .\scripts\load-local-env.ps1
$benchmarkRuntime = Join-Path (Get-Location) "build\core-windows-mingw-benchmarks-debug\vcpkg_installed\x64-mingw-dynamic\debug\bin"
$mingwRuntime = Join-Path $env:MINGW_ROOT "bin"
$env:Path = "$benchmarkRuntime;$mingwRuntime;$env:Path"

.\build\core-windows-mingw-benchmarks-debug\benchmarks\uburu-storage-fts5-benchmark.exe
```

Each benchmark should record dataset, hardware, configuration, memory budget, and observed result before guiding an architectural decision.

## Content hash

`uburu-content-hash-benchmark` measures SHA-256 throughput for content-addressed documents. The synthetic dataset has fixed size and deterministic content to make compiler, flag, and platform comparisons easier.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 `
  -Command configure `
  -Preset core-windows-mingw-benchmarks-debug

powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 `
  -Command build `
  -Preset core-windows-mingw-benchmarks-debug

.\build\core-windows-mingw-benchmarks-debug\benchmarks\uburu-content-hash-benchmark.exe
```
