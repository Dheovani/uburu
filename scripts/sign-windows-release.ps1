param(
  [string]$PackageDirectory = "dist/windows-msvc-release/uburu-windows-msvc-x64",
  [string]$InstallerDirectory = "dist/windows-msvc-release/installer",
  [string]$CertificatePath,
  [string]$CertificatePassword,
  [string]$TimestampUrl = "http://timestamp.digicert.com",
  [string]$SignToolPath
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

function Find-SignTool {
  if ($SignToolPath) {
    return $SignToolPath
  }

  $fromPath = Get-Command signtool.exe -ErrorAction SilentlyContinue

  if ($fromPath) {
    return $fromPath.Source
  }

  $kitsRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
  if (Test-Path -LiteralPath $kitsRoot) {
    $candidate = Get-ChildItem -LiteralPath $kitsRoot -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
      Where-Object { $_.FullName -match "\\x64\\signtool\.exe$" } |
      Sort-Object -Property FullName -Descending |
      Select-Object -First 1

    if ($candidate) {
      return $candidate.FullName
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

function Invoke-SignTool {
  param([string]$TargetPath)

  $arguments = @(
    "sign",
    "/fd", "SHA256",
    "/tr", $TimestampUrl,
    "/td", "SHA256",
    "/f", $certificateFullPath
  )

  if ($CertificatePassword) {
    $arguments += @("/p", $CertificatePassword)
  }

  $arguments += $TargetPath

  & $signTool @arguments

  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

function Write-Checksum {
  param([string]$TargetPath)

  $hash = Get-FileHash -LiteralPath $TargetPath -Algorithm SHA256
  $checksumPath = "$TargetPath.sha256"
  "$($hash.Hash.ToLowerInvariant())  $(Split-Path -Leaf $TargetPath)" |
    Set-Content -LiteralPath $checksumPath -Encoding ASCII
}

if (-not $CertificatePath) {
  throw "CertificatePath is required. Provide a code-signing .pfx file."
}

$packagePath = Join-Path $root $PackageDirectory
$installerPath = Join-Path $root $InstallerDirectory
$certificateFullPath = [System.IO.Path]::GetFullPath((Join-Path $root $CertificatePath))
$signTool = Find-SignTool

if (-not $signTool) {
  throw "signtool.exe was not found. Install the Windows SDK or pass -SignToolPath."
}

if (-not (Test-Path -LiteralPath $certificateFullPath)) {
  throw "Certificate file not found: $certificateFullPath"
}

Assert-PathInsideRoot $packagePath
Assert-PathInsideRoot $installerPath

$targets = @()
$desktopExecutable = Join-Path $packagePath "uburu_desktop.exe"

if (Test-Path -LiteralPath $desktopExecutable) {
  $targets += $desktopExecutable
}

$installer = Get-ChildItem -LiteralPath $installerPath -Filter "uburu-setup-*-windows-x64.exe" |
  Sort-Object -Property LastWriteTimeUtc -Descending |
  Select-Object -First 1

if ($installer) {
  $targets += $installer.FullName
}

if ($targets.Count -eq 0) {
  throw "No Windows release binaries were found to sign."
}

foreach ($target in $targets) {
  Invoke-SignTool -TargetPath $target
  Write-Checksum -TargetPath $target
  Write-Output "Signed: $target"
}

Write-Output "Rebuild ZIP and installer artifacts after signing package binaries if the signed executable must be inside those archives."
