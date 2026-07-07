param(
  [Parameter(Mandatory = $true)]
  [string]$Results,

  [Parameter(Mandatory = $true)]
  [string]$Baseline,

  [switch]$AllowMissing
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Read-JsonFile {
  param([Parameter(Mandatory = $true)][string]$Path)

  if (-not (Test-Path -LiteralPath $Path)) {
    throw "File not found: $Path"
  }

  return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-BenchmarkMetric {
  param(
    [Parameter(Mandatory = $true)]$Benchmark,
    [Parameter(Mandatory = $true)][string]$Metric
  )

  $property = $Benchmark.PSObject.Properties[$Metric]
  if ($null -eq $property) {
    return $null
  }

  return [double]$property.Value
}

function Find-Benchmark {
  param(
    [Parameter(Mandatory = $true)]$Benchmarks,
    [Parameter(Mandatory = $true)][string]$Name
  )

  $exact = @($Benchmarks | Where-Object { $_.name -eq $Name })
  if ($exact.Count -gt 0) {
    return $exact[0]
  }

  $prefixed = @($Benchmarks | Where-Object { $_.name -like "$Name/*" })
  if ($prefixed.Count -gt 0) {
    return $prefixed[0]
  }

  return $null
}

$resultsJson = Read-JsonFile -Path $Results
$baselineJson = Read-JsonFile -Path $Baseline

if ($null -eq $resultsJson.benchmarks) {
  throw "Results file does not contain a Google Benchmark 'benchmarks' array."
}

if ($null -eq $baselineJson.checks) {
  throw "Baseline file does not contain a 'checks' array."
}

$failures = 0

foreach ($check in $baselineJson.checks) {
  $benchmark = Find-Benchmark -Benchmarks $resultsJson.benchmarks -Name $check.benchmark
  if ($null -eq $benchmark) {
    $message = "MISSING benchmark=$($check.benchmark)"
    if ($AllowMissing) {
      Write-Host $message
      continue
    }

    Write-Host $message
    $failures += 1
    continue
  }

  $value = Get-BenchmarkMetric -Benchmark $benchmark -Metric $check.metric
  if ($null -eq $value) {
    $message = "MISSING metric=$($check.metric) benchmark=$($check.benchmark)"
    if ($AllowMissing) {
      Write-Host $message
      continue
    }

    Write-Host $message
    $failures += 1
    continue
  }

  if ($null -ne $check.PSObject.Properties["maximum"]) {
    $maximum = [double]$check.maximum
    if ($value -gt $maximum) {
      Write-Host "FAIL benchmark=$($check.benchmark) metric=$($check.metric) value=$value maximum=$maximum"
      $failures += 1
      continue
    }
  }

  if ($null -ne $check.PSObject.Properties["minimum"]) {
    $minimum = [double]$check.minimum
    if ($value -lt $minimum) {
      Write-Host "FAIL benchmark=$($check.benchmark) metric=$($check.metric) value=$value minimum=$minimum"
      $failures += 1
      continue
    }
  }

  Write-Host "PASS benchmark=$($check.benchmark) metric=$($check.metric) value=$value"
}

if ($failures -gt 0) {
  throw "$failures benchmark baseline check(s) failed."
}
