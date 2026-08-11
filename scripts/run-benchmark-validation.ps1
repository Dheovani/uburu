param(
  [string]$BuildDirectory = "build/core-windows-msvc-debug",
  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Release",
  [string]$Output = "build/benchmark-validation-results.json",
  [string]$Baseline = "benchmarks/baselines/reference-developer.json",
  [ValidateRange(1, 20)]
  [int]$Repetitions = 5,
  [ValidateRange(0.01, 60.0)]
  [double]$MinimumTimeSeconds = 0.2,
  [switch]$SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDirectory = Join-Path $projectRoot $BuildDirectory
$resolvedOutput = Join-Path $projectRoot $Output
$resolvedBaseline = Join-Path $projectRoot $Baseline
$benchmarkName = "uburu-search-service-benchmark"
$benchmarkFileName = if ($env:OS -eq "Windows_NT") { "$benchmarkName.exe" } else { $benchmarkName }

if (-not $SkipBuild) {
  & cmake --build $resolvedBuildDirectory --config $Configuration --target $benchmarkName
  if ($LASTEXITCODE -ne 0) {
    throw "Benchmark target build failed with exit code $LASTEXITCODE."
  }
}

$benchmarkCandidates = @(
  (Join-Path $resolvedBuildDirectory "benchmarks/$Configuration/$benchmarkFileName"),
  (Join-Path $resolvedBuildDirectory "benchmarks/$benchmarkFileName")
)
$benchmarkExecutable = $benchmarkCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if ($null -eq $benchmarkExecutable) {
  throw "Benchmark executable not found under $resolvedBuildDirectory."
}

$outputDirectory = Split-Path -Parent $resolvedOutput
if (-not (Test-Path -LiteralPath $outputDirectory)) {
  New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}

$filter = "^(BM_SearchService_Direct_ManySmallFiles_Literal|" +
  "BM_SearchService_Direct_FewLargeFiles_Literal|" +
  "BM_SearchService_Direct_SparseMatchLargeFiles_Literal_WorkerScaling/(1|8)|" +
  "BM_SearchService_Direct_ManySmallFiles_Regex|" +
  "BM_SearchService_Batching_Adaptive_RenderCost|" +
  "BM_IndexService_Initial_ManySmallFiles|" +
  "BM_IndexService_Incremental_ManySmallFiles|" +
  "BM_IndexService_BranchSwitch_ManySmallFiles|" +
  "BM_IndexService_BlobHashReuse_RenamedFile)$"

Write-Host "Running repeatable benchmark validation: $benchmarkExecutable"
& $benchmarkExecutable `
  "--benchmark_filter=$filter" `
  "--benchmark_min_time=${MinimumTimeSeconds}s" `
  "--benchmark_repetitions=$Repetitions" `
  "--benchmark_report_aggregates_only=true" `
  "--benchmark_out=$resolvedOutput" `
  "--benchmark_out_format=json"

if ($LASTEXITCODE -ne 0) {
  throw "Benchmark execution failed with exit code $LASTEXITCODE."
}

& (Join-Path $PSScriptRoot "check-benchmark-baseline.ps1") `
  -Results $resolvedOutput `
  -Baseline $resolvedBaseline

if ($LASTEXITCODE -ne 0) {
  throw "Benchmark baseline validation failed with exit code $LASTEXITCODE."
}

Write-Host "Benchmark validation passed. Results: $resolvedOutput"
