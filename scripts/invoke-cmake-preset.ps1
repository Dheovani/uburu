param(
  [ValidateSet("configure", "build", "test", "format", "format-check", "tidy")]
  [string]$Command = "build",
  [string]$Preset = "local-windows-msvc-debug",
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

function Test-MsvcPresetEnvironment {
  param([string]$RuntimePreset)

  if ($RuntimePreset -notlike "*msvc*") {
    return
  }

  Test-RequiredPath "VCPKG_ROOT" $env:VCPKG_ROOT "C:\Users\your-user\vcpkg"

  if ($RuntimePreset -notlike "core-*") {
    Test-RequiredPath "QT_ROOT" $env:QT_ROOT "C:\Qt\6.11.1\msvc2022_64"
  }
}

function Add-RuntimePathForPreset {
  param([string]$RuntimePreset)

  $buildDirectory = switch ($RuntimePreset) {
    "local-windows-msvc-debug" { "build/windows-msvc-debug" }
    "windows-msvc-debug" { "build/windows-msvc-debug" }
    "core-windows-msvc-debug" { "build/core-windows-msvc-debug" }
    "core-windows-msvc-werror-debug" { "build/core-windows-msvc-werror-debug" }
    default { "" }
  }

  if (-not $buildDirectory) {
    return
  }

  $root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
  $buildPath = Join-Path $root $buildDirectory

  Add-PathEntry (Join-Path $buildPath "vcpkg_installed/x64-windows/debug/bin")

  if ($env:QT_ROOT) {
    Add-PathEntry (Join-Path $env:QT_ROOT "bin")
  }
}

if ($env:QT_ROOT) {
  Add-PathEntry (Join-Path $env:QT_ROOT "bin")
}

Add-PathEntry $env:NINJA_ROOT

Test-MsvcPresetEnvironment $Preset

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
