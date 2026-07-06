# Benchmarks

This directory contains reproducible development benchmarks. They stay outside the default build so they do not slow down compilation, tests, or CI.

## Search service

`uburu-search-service-benchmark` uses Google Benchmark to measure `DefaultSearchService` through deterministic direct-search scenarios and the first persistent indexing scenarios. It records Google Benchmark timings and Uburu-specific counters for matches, scanned files, processed bytes, throughput, time to first result, binary skips, ignored files, result batches, approximate memory, whether a regex scenario used PCRE2 JIT, indexed documents, catalog reuse, hash reuse, and branch-switch staleness.

The current scenarios cover many small files, few large files, case-insensitive literal search, case-sensitive literal search, whole-word literal search, precomposed-versus-decomposed Unicode normalization cost, regex/JIT search, `.gitignore`-heavy traversal, mixed text/binary filtering, and repeated scans over the same dataset.

The repeated-scan scenario does not flush the operating-system cache. It compares the first observed pass with an immediate second pass and publishes `first_pass_*`, `second_pass_*`, and `second_pass_speedup` counters. Treat it as a practical cache-effect signal, not as a laboratory-grade cold-cache measurement.

The persistent-index scenarios use a disposable SQLite database and deterministic temporary files. Initial indexing measures a fresh generation. Incremental indexing prepares the first generation with benchmark timing paused, then measures the unchanged second update. Branch-switch indexing also prepares the first generation with timing paused, then measures staleness detection plus update against a changed branch and HEAD. Dedicated reuse scenarios measure content-hash deduplication and Git blob-hash reuse for a renamed file candidate.

```powershell
cmake --preset core-windows-msvc-debug -DUBURU_BUILD_BENCHMARKS=ON
cmake --build build/core-windows-msvc-debug --config Debug --target uburu-search-service-benchmark

.\build\core-windows-msvc-debug\benchmarks\Debug\uburu-search-service-benchmark.exe
```

For JSON output:

```powershell
.\build\core-windows-msvc-debug\benchmarks\Debug\uburu-search-service-benchmark.exe `
  --benchmark_format=json `
  --benchmark_out=benchmark-results.json
```

Benchmarks are developer tools, not correctness tests, and should not be registered as normal CTest cases.

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
