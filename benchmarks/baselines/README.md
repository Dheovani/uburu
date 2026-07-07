# Benchmark baselines

This directory stores versioned benchmark baselines and guardrails for Uburu performance work.

Baselines are not universal truth. They are reference snapshots for a named hardware class, toolchain, build type, dataset profile, and benchmark target. A slower or faster machine should get its own baseline file instead of silently changing an existing one.

Use `scripts/check-benchmark-baseline.ps1` to compare Google Benchmark JSON output against a baseline file:

```powershell
.\build\core-windows-msvc-debug\benchmarks\Debug\uburu-search-service-benchmark.exe `
  --benchmark_format=json `
  --benchmark_out=benchmark-results.json

powershell -ExecutionPolicy Bypass -File .\scripts\check-benchmark-baseline.ps1 `
  -Results benchmark-results.json `
  -Baseline benchmarks\baselines\reference-developer.json
```

The initial `reference-developer.json` is intentionally conservative. It exists to catch order-of-magnitude regressions and missing counters while the project is still evolving. Tighten or add hardware-specific baselines only after repeated runs on the same machine show stable ranges.
