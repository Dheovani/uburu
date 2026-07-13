param(
  [string]$PackageScript = "scripts/package-windows-msvc-desktop.ps1",
  [string]$InstallerScript = "scripts/build-windows-installer.ps1",
  [string]$SbomScript = "scripts/generate-release-sbom.ps1",
  [string]$OutputDirectory = "dist/windows-msvc-release",
  [string]$PackageName = "uburu-windows-msvc-x64",
  [string]$ReleaseNotes = "docs/releases/v0.1.0.md",
  [string]$AppVersion,
  [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

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

function Assert-PathInsideRoot {
  param([string]$Path)

  $rootPath = [System.IO.Path]::GetFullPath($root)
  $targetPath = [System.IO.Path]::GetFullPath($Path)

  if (-not $targetPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to operate outside the repository: $targetPath"
  }
}

function ConvertTo-RepoRelativePath {
  param([string]$Path)

  $rootPath = [System.IO.Path]::GetFullPath($root)
  $targetPath = [System.IO.Path]::GetFullPath($Path)

  if (-not $rootPath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
    $rootPath = "$rootPath$([System.IO.Path]::DirectorySeparatorChar)"
  }

  $rootUri = [System.Uri]::new($rootPath)
  $targetUri = [System.Uri]::new($targetPath)
  $relativeUri = $rootUri.MakeRelativeUri($targetUri)

  return [System.Uri]::UnescapeDataString($relativeUri.ToString()) -replace '/', [System.IO.Path]::DirectorySeparatorChar
}

function New-ReleaseAsset {
  param(
    [string]$Path,
    [string]$ChecksumPath,
    [string]$Kind
  )

  if (-not (Test-Path -LiteralPath $Path)) {
    throw "Release asset not found: $Path"
  }

  $item = Get-Item -LiteralPath $Path
  $hash = Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256

  if ($ChecksumPath) {
    "$($hash.Hash.ToLowerInvariant())  $($item.Name)" | Set-Content -LiteralPath $ChecksumPath -Encoding ASCII
  }

  return [ordered]@{
    kind = $Kind
    name = $item.Name
    path = ConvertTo-RepoRelativePath -Path $item.FullName
    bytes = $item.Length
    sha256 = $hash.Hash.ToLowerInvariant()
    checksumPath = if ($ChecksumPath) { ConvertTo-RepoRelativePath -Path $ChecksumPath } else { $null }
  }
}

if (-not $AppVersion) {
  $AppVersion = Git-Value -Arguments @("describe", "--tags", "--always")
}

$safeAppVersion = $AppVersion -replace '[^0-9A-Za-z.\-_]', '-'
$packageScriptPath = Join-Path $root $PackageScript
$installerScriptPath = Join-Path $root $InstallerScript
$sbomScriptPath = Join-Path $root $SbomScript
$outputPath = Join-Path $root $OutputDirectory
$installerOutputPath = Join-Path $outputPath "installer"
$releaseNotesPath = Join-Path $root $ReleaseNotes
$portableArchivePath = Join-Path $outputPath "$PackageName.zip"
$portableChecksumPath = Join-Path $outputPath "$PackageName.sha256"
$releaseNotesOutputPath = Join-Path $outputPath "RELEASE-NOTES.md"
$assetsManifestPath = Join-Path $outputPath "release-assets.json"
$sbomPath = Join-Path $outputPath "uburu-windows-msvc-x64.spdx.json"
$licenseReportPath = Join-Path $outputPath "THIRD-PARTY-NOTICES.md"

Assert-PathInsideRoot $outputPath
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

if (-not $SkipPackage) {
  & $packageScriptPath

  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

& $installerScriptPath -SkipPackage -AppVersion $safeAppVersion

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $releaseNotesPath)) {
  throw "Release notes not found: $releaseNotesPath"
}

Copy-Item -LiteralPath $releaseNotesPath -Destination $releaseNotesOutputPath -Force

& $sbomScriptPath

if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$installer = Get-ChildItem -LiteralPath $installerOutputPath -Filter "uburu-setup-*-windows-x64.exe" |
  Sort-Object -Property LastWriteTimeUtc -Descending |
  Select-Object -First 1

if (-not $installer) {
  throw "Installer was not generated in $installerOutputPath."
}

$assets = @(
  (New-ReleaseAsset `
      -Kind "windows-installer" `
      -Path $installer.FullName `
      -ChecksumPath "$($installer.FullName).sha256"),
  (New-ReleaseAsset `
      -Kind "windows-portable-zip" `
      -Path $portableArchivePath `
      -ChecksumPath $portableChecksumPath),
  (New-ReleaseAsset `
      -Kind "spdx-sbom" `
      -Path $sbomPath `
      -ChecksumPath "$sbomPath.sha256"),
  (New-ReleaseAsset `
      -Kind "third-party-notices" `
      -Path $licenseReportPath `
      -ChecksumPath "$licenseReportPath.sha256"),
  (New-ReleaseAsset `
      -Kind "release-notes" `
      -Path $releaseNotesOutputPath `
      -ChecksumPath $null)
)

$manifest = [ordered]@{
  name = "Uburu"
  version = $safeAppVersion
  createdAtUtc = (Get-Date).ToUniversalTime().ToString("o")
  assets = $assets
}

$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $assetsManifestPath -Encoding UTF8

Write-Output "Release assets manifest: $assetsManifestPath"
foreach ($asset in $assets) {
  Write-Output "$($asset.kind): $($asset.path)"
}
