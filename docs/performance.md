# Performance

Time to first result is the primary metric. Direct search streams results while reading files line by line and does not wait for scanning to finish. UI delivery uses adaptive batches to reduce overhead without hurting initial latency.

## Minimum metrics

- scan time and total time;
- time to first result;
- files and bytes per second;
- matching time;
- hidden files filtered by the scanner;
- files ignored by `.gitignore`, `.git/info/exclude`, or configured global ignores;
- binary files detected by the text reader and binary files actually skipped;
- approximate peak memory;
- documents indexed and reused by hash.

In the current state, `SearchSummary::metrics` aggregates basic direct-search counters. The scanner increments hidden and ignored files when it discards entries before publishing them to the engine. The text reader identifies binaries by sampling, and the engine records those files as skipped binaries when `SearchOptions::includeBinary` does not allow textual reading. Ignored directories may prevent descendant enumeration, so counters represent files directly observed during traversal, not a recursive estimate of everything under the directory.

`SearchService::searchWithEvents()` measures `timeToFirstResult` and `totalTime` at the selected search strategy level. The service also measures the synchronous latency of delivering each batch to the sink and adjusts the next batch size within `SearchExecutionOptions` limits. `StructuredMetricsSink` writes search metrics as a structured event in the `search` category, with numeric fields for times, files, bytes, and results.

The structured logger supports filtering by minimum level and enabled categories. `FileStructuredLogger` writes JSON Lines and rotates by size, keeping a configurable number of old files. Fields marked as sensitive are masked by default; full paths, line content, and potentially private expressions should not be added as public fields without an explicit application-layer decision.

Search metrics include derived throughput (`filesPerSecond` and `bytesPerSecond`), queue wait counters, index cache hits/misses, reuse by catalog/blob/hash, and an estimate of memory occupied by emitted results. The memory estimate does not replace a profiler: it sums approximate result structure sizes and observable string/vector capacities. `SearchService` compares that estimate with the previous search to signal growth between executions.

`DiagnosticReport` defines the initial exportable diagnostics format: structured logs, search metrics, and tracing events in JSON. Fields marked as sensitive remain masked by default in exported reports. The Milestone 8 diagnostics UI should consume this contract instead of reading internal service details directly.

`SearchTraceRecorder` provides opt-in tracing. When disabled, `record()` returns without storing events and `SearchTraceScope` does not publish spans when it leaves scope. When enabled, the recorder limits event count, records span durations, and reuses `LogField` to keep the same sensitive-data masking policy.

The future scanner will use a bounded pool, small-file prioritization, and backpressure. Optimizations must come with reproducible benchmarks for many small files, few large files, literal search, regex, initial indexing, and incremental reconciliation.

## Developer benchmarks

Uburu uses Google Benchmark for developer performance measurements. Benchmarks are disabled by default and can be enabled with `UBURU_BUILD_BENCHMARKS=ON`.

The initial benchmark target is `uburu-search-service-benchmark`. It measures direct search scenarios through `DefaultSearchService`, using deterministic temporary datasets and Uburu-specific counters such as time to first result, total time, throughput, ignored files, binary skips, emitted results, approximate memory usage, and PCRE2 JIT activation for regex scenarios.

Benchmarks are intentionally separate from CTest correctness tests. Run them explicitly:

```powershell
cmake --preset core-windows-msvc-debug -DUBURU_BUILD_BENCHMARKS=ON
cmake --build build/core-windows-msvc-debug --config Debug --target uburu-search-service-benchmark
.\build\core-windows-msvc-debug\benchmarks\Debug\uburu-search-service-benchmark.exe
```

Export JSON results with:

```powershell
.\build\core-windows-msvc-debug\benchmarks\Debug\uburu-search-service-benchmark.exe `
  --benchmark_format=json `
  --benchmark_out=benchmark-results.json
```

Initial scenarios cover many small files, few large files, case-insensitive literal search, case-sensitive literal search, whole-word literal search, precomposed-versus-decomposed Unicode normalization cost, regex/JIT-heavy content, `.gitignore`-heavy trees, and mixed text/binary filtering. Future benchmark targets should reuse `uburu_benchmark_support` for deterministic datasets and consistent counter publication.

## Large file reading

The text reader processes content in chunks and keeps only the current line, pending decoding bytes, and configured context. This avoids allocation proportional to the whole file size for UTF-8, Latin-1, and UTF-16.

`SearchOptions::maximumLineLength` limits extreme lines to prevent uncontrolled memory growth. `binarySampleSize` controls the sample used to detect binaries before matching.

When `contextAfterLines` is greater than zero, results may be retained for a few lines to fill following context. Memory cost is proportional to the number of pending results and configured context, not to the total file size.
