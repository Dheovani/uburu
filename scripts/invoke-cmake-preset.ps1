param(
  [ValidateSet("configure", "build", "test", "format", "format-check", "tidy")]
  [string]$Command = "build",
  [string]$Preset = "windows-mingw-debug"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "load-local-env.ps1")

function Add-PathEntry {
  param([string]$PathEntry)

  if ($PathEntry -and (Test-Path -LiteralPath $PathEntry)) {
    $env:Path = "$PathEntry;$env:Path"
  }
}

function Add-RuntimePathForPreset {
  param([string]$RuntimePreset)

  $buildDirectory = switch ($RuntimePreset) {
    "windows-mingw-debug" { "build/windows-mingw-debug" }
    "core-windows-mingw-debug" { "build/core-windows-mingw-debug" }
    "core-windows-mingw-werror-debug" { "build/core-windows-mingw-werror-debug" }
    "core-windows-mingw-benchmarks-debug" { "build/core-windows-mingw-benchmarks-debug" }
    default { "" }
  }

  if (-not $buildDirectory) {
    return
  }

  $root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
  $buildPath = Join-Path $root $buildDirectory

  Add-PathEntry (Join-Path $buildPath "vcpkg_installed/x64-mingw-dynamic/debug/bin")
  Add-PathEntry (Join-Path $env:QT_ROOT "bin")
  Add-PathEntry (Join-Path $env:MINGW_ROOT "bin")
}

Add-PathEntry (Join-Path $env:MINGW_ROOT "bin")
Add-PathEntry (Join-Path $env:QT_ROOT "bin")
Add-PathEntry $env:NINJA_ROOT

switch ($Command) {
  "configure" {
    & cmake --preset $Preset
  }
  "build" {
    & cmake --build --preset $Preset
  }
  "test" {
    Add-RuntimePathForPreset $Preset
    & ctest --preset $Preset
  }
  "format" {
    & cmake --build --preset $Preset --target format
  }
  "format-check" {
    & cmake --build --preset $Preset --target format-check
  }
  "tidy" {
    & cmake --build --preset $Preset --target tidy
  }
}

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
