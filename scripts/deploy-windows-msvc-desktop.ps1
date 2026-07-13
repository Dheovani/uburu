param(
  [string]$BuildDirectory = "build/windows-msvc-debug",
  [string]$OutputDirectory = "dist/windows-msvc-debug",
  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
. (Join-Path $PSScriptRoot "load-local-env.ps1")

if (-not $env:QT_ROOT) {
  throw "QT_ROOT is required, for example C:\Qt\6.11.1\msvc2022_64."
}

function Copy-MsvcRuntime {
  $visualStudioRoot = Join-Path $env:ProgramFiles "Microsoft Visual Studio"

  if (-not (Test-Path -LiteralPath $visualStudioRoot)) {
    Write-Warning "Microsoft Visual Studio root not found; MSVC runtime DLLs were not copied."

    return
  }

  $runtimeDirectory = Get-ChildItem -LiteralPath $visualStudioRoot -Directory -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match "\\VC\\Redist\\MSVC\\[^\\]+\\x64\\Microsoft\.VC.*\.CRT$" } |
    Sort-Object -Property FullName -Descending |
    Select-Object -First 1

  if (-not $runtimeDirectory) {
    Write-Warning "MSVC x64 runtime directory not found; install the Visual Studio C++ redistributable components."

    return
  }

  Get-ChildItem -LiteralPath $runtimeDirectory.FullName -Filter "*.dll" |
    Copy-Item -Destination $outputPath -Force
}

$buildPath = Join-Path $root $BuildDirectory
$outputPath = Join-Path $root $OutputDirectory
$executable = Join-Path $buildPath "apps/desktop/$Configuration/uburu_desktop.exe"
$deployedExecutable = Join-Path $outputPath "uburu_desktop.exe"
$vcpkgBin = if ($Configuration -eq "Debug") {
  Join-Path $buildPath "vcpkg_installed/x64-windows/debug/bin"
} else {
  Join-Path $buildPath "vcpkg_installed/x64-windows/bin"
}
$winDeployQt = Join-Path $env:QT_ROOT "bin/windeployqt.exe"
$deployConfigurationFlag = if ($Configuration -eq "Debug") { "--debug" } else { "--release" }

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
  --compiler-runtime `
  $deployConfigurationFlag `
  --no-translations `
  $deployedExecutable

if (Test-Path -LiteralPath $vcpkgBin) {
  Get-ChildItem -LiteralPath $vcpkgBin -Filter "*.dll" |
    Where-Object { $_.Name -notin @("benchmark.dll", "benchmark_main.dll") } |
    Copy-Item -Destination $outputPath -Force
}

Copy-MsvcRuntime

Write-Output "Deployment written to $outputPath"
