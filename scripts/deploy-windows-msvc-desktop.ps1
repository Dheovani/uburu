param(
  [string]$BuildDirectory = "build/windows-msvc-debug",
  [string]$OutputDirectory = "dist/windows-msvc-debug"
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
. (Join-Path $PSScriptRoot "load-local-env.ps1")

if (-not $env:QT_ROOT) {
  throw "QT_ROOT is required, for example C:\Qt\6.11.1\msvc2022_64."
}

$buildPath = Join-Path $root $BuildDirectory
$outputPath = Join-Path $root $OutputDirectory
$executable = Join-Path $buildPath "apps/desktop/Debug/uburu_desktop.exe"
$deployedExecutable = Join-Path $outputPath "uburu_desktop.exe"
$vcpkgBin = Join-Path $buildPath "vcpkg_installed/x64-windows/debug/bin"
$winDeployQt = Join-Path $env:QT_ROOT "bin/windeployqt.exe"

if (-not (Test-Path -LiteralPath $executable)) {
  throw "Desktop executable not found at $executable. Build the local-windows-msvc-debug preset first."
}

if (-not (Test-Path -LiteralPath $winDeployQt)) {
  throw "windeployqt not found at $winDeployQt. Check QT_ROOT."
}

New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
Copy-Item -LiteralPath $executable -Destination $deployedExecutable -Force

& $winDeployQt `
  --qmldir (Join-Path $root "apps/desktop/qml") `
  --plugindir (Join-Path $outputPath "plugins") `
  --no-translations `
  $deployedExecutable

if (Test-Path -LiteralPath $vcpkgBin) {
  Get-ChildItem -LiteralPath $vcpkgBin -Filter "*.dll" |
    Copy-Item -Destination $outputPath -Force
}

Write-Output "Deployment written to $outputPath"
