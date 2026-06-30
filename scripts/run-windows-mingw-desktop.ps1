param(
  [switch]$SkipBuild,
  [switch]$SkipRun
)

$ErrorActionPreference = "Stop"

Write-Warning "MinGW is no longer the supported local Windows toolchain. Forwarding to MSVC."

$targetScript = Join-Path $PSScriptRoot "run-windows-msvc-desktop.ps1"

if ($SkipBuild -and $SkipRun) {
  & $targetScript -SkipBuild -SkipRun
} elseif ($SkipBuild) {
  & $targetScript -SkipBuild
} elseif ($SkipRun) {
  & $targetScript -SkipRun
} else {
  & $targetScript
}
