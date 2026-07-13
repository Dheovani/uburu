param(
  [string]$PackageScript = "scripts/package-windows-msvc-desktop.ps1",
  [string]$PackageDirectory = "dist/windows-msvc-release/uburu-windows-msvc-x64",
  [string]$InstallerOutputDirectory = "dist/windows-msvc-release/installer",
  [string]$InstallerDefinition = "packaging/windows/uburu-installer.iss",
  [string]$AppVersion,
  [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
. (Join-Path $PSScriptRoot "load-local-env.ps1")

function Git-Value {
  param([string[]]$Arguments)

  try {
    $value = & git @Arguments 2>$null

    if ($LASTEXITCODE -eq 0 -and $value) {
      return ($value | Select-Object -First 1)
    }
  } catch {
  }

  return "0.1.0-dev"
}

function Find-InnoSetupCompiler {
  $fromPath = Get-Command iscc.exe -ErrorAction SilentlyContinue

  if ($fromPath) {
    return $fromPath.Source
  }

  $candidates = @(
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
  )

  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path -LiteralPath $candidate)) {
      return $candidate
    }
  }

  return $null
}

function Assert-PathInsideRoot {
  param([string]$Path)

  $rootPath = [System.IO.Path]::GetFullPath($root)
  $targetPath = [System.IO.Path]::GetFullPath($Path)

  if (-not $targetPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to operate outside the repository: $targetPath"
  }
}

$packageScriptPath = Join-Path $root $PackageScript
$packagePath = Join-Path $root $PackageDirectory
$installerDefinitionPath = Join-Path $root $InstallerDefinition
$installerOutputPath = Join-Path $root $InstallerOutputDirectory
$innoSetupCompiler = Find-InnoSetupCompiler

if (-not $innoSetupCompiler) {
  throw "Inno Setup compiler not found. Install Inno Setup 6 and rerun this script, or add ISCC.exe to PATH."
}

if (-not $SkipPackage) {
  & $packageScriptPath

  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

if (-not (Test-Path -LiteralPath $packagePath)) {
  throw "Package directory not found at $packagePath. Run package-windows-msvc-desktop.ps1 first."
}

if (-not (Test-Path -LiteralPath $installerDefinitionPath)) {
  throw "Installer definition not found at $installerDefinitionPath."
}

if (-not $AppVersion) {
  $AppVersion = Git-Value -Arguments @("describe", "--tags", "--always")
}

$safeAppVersion = $AppVersion -replace '[^0-9A-Za-z.\-_]', '-'

Assert-PathInsideRoot $installerOutputPath
New-Item -ItemType Directory -Force -Path $installerOutputPath | Out-Null

& $innoSetupCompiler `
  "/DPackageDir=$packagePath" `
  "/DOutputDir=$installerOutputPath" `
  "/DAppVersion=$safeAppVersion" `
  $installerDefinitionPath

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$installer = Get-ChildItem -LiteralPath $installerOutputPath -Filter "uburu-setup-*-windows-x64.exe" |
  Sort-Object -Property LastWriteTimeUtc -Descending |
  Select-Object -First 1

if (-not $installer) {
  throw "Installer was not generated in $installerOutputPath."
}

$hash = Get-FileHash -LiteralPath $installer.FullName -Algorithm SHA256
$checksumPath = "$($installer.FullName).sha256"
"$($hash.Hash.ToLowerInvariant())  $($installer.Name)" | Set-Content -LiteralPath $checksumPath -Encoding ASCII

Write-Output "Installer: $($installer.FullName)"
Write-Output "SHA-256: $checksumPath"
