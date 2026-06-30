param(
  [ValidateSet("configure", "build", "test", "format", "format-check", "tidy")]
  [string]$Command = "build",
  [string]$Preset = "windows-mingw-debug",
  [switch]$Fresh
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "load-local-env.ps1")

function Add-PathEntry {
  param([string]$PathEntry)

  if ($PathEntry -and (Test-Path -LiteralPath $PathEntry)) {
    $env:Path = "$PathEntry;$env:Path"
  }
}

function Test-RequiredPath {
  param(
    [string]$Name,
    [string]$PathValue,
    [string]$Example
  )

  if (-not $PathValue) {
    throw "$Name is required, for example $Example."
  }

  if (-not (Test-Path -LiteralPath $PathValue)) {
    throw "$Name points to '$PathValue', but that path does not exist."
  }
}

function Test-MingwPresetEnvironment {
  param([string]$RuntimePreset)

  if ($RuntimePreset -notlike "*mingw*") {
    return
  }

  Test-RequiredPath "MINGW_ROOT" $env:MINGW_ROOT "C:\Qt\Tools\mingw1310_64"
  Test-RequiredPath "VCPKG_ROOT" $env:VCPKG_ROOT "C:\Users\your-user\vcpkg"

  if ($RuntimePreset -notlike "core-*") {
    Test-RequiredPath "QT_ROOT" $env:QT_ROOT "C:\Qt\6.11.1\mingw_64"
  }

  $compiler = Join-Path $env:MINGW_ROOT "bin/g++.exe"
  if (-not (Test-Path -LiteralPath $compiler)) {
    throw "MinGW compiler was not found at '$compiler'. Check MINGW_ROOT in .env."
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

Test-MingwPresetEnvironment $Preset

switch ($Command) {
  "configure" {
    if ($Fresh) {
      & cmake --fresh --preset $Preset
    } else {
      & cmake --preset $Preset
    }
  }
  "build" {
    if ($Fresh) {
      & cmake --fresh --preset $Preset

      if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
      }
    }

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
