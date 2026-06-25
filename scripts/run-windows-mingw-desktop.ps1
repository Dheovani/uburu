param(
  [string]$BuildDirectory = "build/windows-mingw-debug"
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
. (Join-Path $PSScriptRoot "load-local-env.ps1")

if (-not $env:QT_ROOT) {
  throw "QT_ROOT is required, for example C:\Qt\6.11.1\mingw_64."
}

if (-not $env:MINGW_ROOT) {
  throw "MINGW_ROOT is required, for example C:\Qt\Tools\mingw1310_64."
}

$buildPath = Join-Path $root $BuildDirectory
$executable = Join-Path $buildPath "apps/desktop/uburu_desktop.exe"
$vcpkgBin = Join-Path $buildPath "vcpkg_installed/x64-mingw-dynamic/debug/bin"
$qtBin = Join-Path $env:QT_ROOT "bin"
$mingwBin = Join-Path $env:MINGW_ROOT "bin"

if (-not (Test-Path -LiteralPath $executable)) {
  throw "Desktop executable not found at $executable. Build the windows-mingw-debug preset first."
}

$env:Path = "$vcpkgBin;$qtBin;$mingwBin;$env:Path"
& $executable
