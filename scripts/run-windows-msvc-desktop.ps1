param(
  [string]$BuildDirectory = "build/windows-msvc-debug",
  [string]$Preset = "local-windows-msvc-debug",
  [switch]$SkipBuild,
  [switch]$SkipRun
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
. (Join-Path $PSScriptRoot "load-local-env.ps1")

if (-not $env:QT_ROOT) {
  throw "QT_ROOT is required, for example C:\Qt\6.11.1\msvc2022_64."
}

$buildPath = Join-Path $root $BuildDirectory
$executable = Join-Path $buildPath "apps/desktop/Debug/uburu_desktop.exe"
$vcpkgBin = Join-Path $buildPath "vcpkg_installed/x64-windows/debug/bin"
$qtBin = Join-Path $env:QT_ROOT "bin"

function Add-PathEntry {
  param([string]$PathEntry)

  if ($PathEntry -and (Test-Path -LiteralPath $PathEntry)) {
    $env:Path = "$PathEntry;$env:Path"
  }
}

Add-PathEntry $qtBin

if (-not $SkipBuild) {
  Push-Location $root
  try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
      exit $LASTEXITCODE
    }

    & cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) {
      exit $LASTEXITCODE
    }
  } finally {
    Pop-Location
  }
}

if (-not (Test-Path -LiteralPath $executable)) {
  throw "Desktop executable not found at $executable after building preset $Preset."
}

Add-PathEntry $vcpkgBin
Add-PathEntry $qtBin

if (-not $SkipRun) {
  & $executable
}
