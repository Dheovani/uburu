param(
  [string]$Preset = "windows-msvc-release",
  [string]$BuildDirectory = "build/windows-msvc-release",
  [string]$OutputDirectory = "dist/windows-msvc-release",
  [string]$PackageName = "uburu-windows-msvc-x64",
  [switch]$SkipBuild,
  [switch]$SkipArchive
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
. (Join-Path $PSScriptRoot "load-local-env.ps1")

if (-not $env:QT_ROOT) {
  throw "QT_ROOT is required, for example C:\Qt\6.11.1\msvc2022_64."
}

if (-not $env:VCPKG_ROOT) {
  throw "VCPKG_ROOT is required, for example C:\Users\your-user\vcpkg."
}

$outputRoot = Join-Path $root $OutputDirectory
$packagePath = Join-Path $outputRoot $PackageName
$archivePath = Join-Path $outputRoot "$PackageName.zip"
$checksumPath = Join-Path $outputRoot "$PackageName.sha256"
$manifestPath = Join-Path $packagePath "release-manifest.json"

function Assert-PathInsideRoot {
  param([string]$Path)

  $rootPath = [System.IO.Path]::GetFullPath($root)
  $targetPath = [System.IO.Path]::GetFullPath($Path)

  if (-not $targetPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to operate outside the repository: $targetPath"
  }
}

function Git-Value {
  param([string[]]$Arguments)

  try {
    $value = & git @Arguments 2>$null

    if ($LASTEXITCODE -eq 0 -and $value) {
      return ($value | Select-Object -First 1)
    }
  } catch {
  }

  return "unknown"
}

if (-not $SkipBuild) {
  Push-Location $root
  try {
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
      exit $LASTEXITCODE
    }

    & cmake --build --preset $Preset --target uburu_desktop
    if ($LASTEXITCODE -ne 0) {
      exit $LASTEXITCODE
    }
  } finally {
    Pop-Location
  }
}

Assert-PathInsideRoot $outputRoot
Assert-PathInsideRoot $packagePath
Assert-PathInsideRoot $archivePath

if (Test-Path -LiteralPath $packagePath) {
  Remove-Item -LiteralPath $packagePath -Recurse -Force
}

if (Test-Path -LiteralPath $archivePath) {
  Remove-Item -LiteralPath $archivePath -Force
}

if (Test-Path -LiteralPath $checksumPath) {
  Remove-Item -LiteralPath $checksumPath -Force
}

& (Join-Path $PSScriptRoot "deploy-windows-msvc-desktop.ps1") `
  -BuildDirectory $BuildDirectory `
  -OutputDirectory (Join-Path $OutputDirectory $PackageName) `
  -Configuration Release

Copy-Item -LiteralPath (Join-Path $root "README.md") -Destination $packagePath -Force
Copy-Item -LiteralPath (Join-Path $root "README.pt-BR.md") -Destination $packagePath -Force
Copy-Item -LiteralPath (Join-Path $root "LICENSE") -Destination $packagePath -Force
Copy-Item -LiteralPath (Join-Path $root "docs/licenses.md") -Destination $packagePath -Force

$manifest = [ordered]@{
  name = "Uburu"
  package = $PackageName
  version = Git-Value -Arguments @("describe", "--tags", "--always", "--dirty")
  commit = Git-Value -Arguments @("rev-parse", "HEAD")
  configuration = "Release"
  generatorPreset = $Preset
  qtRoot = $env:QT_ROOT
  vcpkgRoot = $env:VCPKG_ROOT
  createdAtUtc = (Get-Date).ToUniversalTime().ToString("o")
}

$manifest | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

if (-not $SkipArchive) {
  Compress-Archive -LiteralPath $packagePath -DestinationPath $archivePath -Force
  $hash = Get-FileHash -LiteralPath $archivePath -Algorithm SHA256
  "$($hash.Hash.ToLowerInvariant())  $(Split-Path -Leaf $archivePath)" |
    Set-Content -LiteralPath $checksumPath -Encoding ASCII

  Write-Output "Package directory: $packagePath"
  Write-Output "Archive: $archivePath"
  Write-Output "SHA-256: $checksumPath"
} else {
  Write-Output "Package directory: $packagePath"
}
