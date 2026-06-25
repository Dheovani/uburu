param(
  [string]$BuildDirectory = "build/windows-mingw-debug",
  [string]$Preset = "windows-mingw-debug",
  [switch]$SkipBuild,
  [switch]$SkipRun
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
$ninjaBin = $env:NINJA_ROOT

function Add-PathEntry {
  param([string]$PathEntry)

  if ($PathEntry -and (Test-Path -LiteralPath $PathEntry)) {
    $env:Path = "$PathEntry;$env:Path"
  }
}

function Get-CachedCompiler {
  param([string]$CacheFile)

  if (-not (Test-Path -LiteralPath $CacheFile)) {
    return ""
  }

  $compilerLine = Get-Content -LiteralPath $CacheFile |
    Where-Object { $_ -match "^CMAKE_CXX_COMPILER:[^=]*=(.*)$" } |
    Select-Object -First 1

  if (-not $compilerLine) {
    return ""
  }

  return ($compilerLine -replace "^CMAKE_CXX_COMPILER:[^=]*=", "")
}

function Test-CachedCompilerIsInvalid {
  param([string]$CacheFile)

  $cachedCompiler = Get-CachedCompiler $CacheFile

  return $cachedCompiler -and -not (Test-Path -LiteralPath $cachedCompiler)
}

Add-PathEntry $mingwBin
Add-PathEntry $qtBin
Add-PathEntry $ninjaBin

if (-not $SkipBuild) {
  $cacheFile = Join-Path $buildPath "CMakeCache.txt"
  $configureArguments = @("--preset", $Preset)

  if (Test-CachedCompilerIsInvalid $cacheFile) {
    $configureArguments = @("--fresh") + $configureArguments
  }

  Push-Location $root
  try {
    & cmake @configureArguments
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
Add-PathEntry $mingwBin

if (-not $SkipRun) {
  & $executable
}
