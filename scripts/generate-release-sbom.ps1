param(
  [string]$PackageDirectory = "dist/windows-msvc-release/uburu-windows-msvc-x64",
  [string]$OutputDirectory = "dist/windows-msvc-release",
  [string]$SbomFileName = "uburu-windows-msvc-x64.spdx.json",
  [string]$LicenseReportFileName = "THIRD-PARTY-NOTICES.md"
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")

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

function Assert-PathInsideRoot {
  param([string]$Path)

  $rootPath = [System.IO.Path]::GetFullPath($root)
  $targetPath = [System.IO.Path]::GetFullPath($Path)

  if (-not $targetPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to operate outside the repository: $targetPath"
  }
}

function New-PackageIdentifier {
  param([string]$Name)

  return "SPDXRef-Package-$($Name -replace '[^0-9A-Za-z.\-]', '-')"
}

$packagePath = Join-Path $root $PackageDirectory
$outputPath = Join-Path $root $OutputDirectory
$sbomPath = Join-Path $outputPath $SbomFileName
$licenseReportPath = Join-Path $outputPath $LicenseReportFileName
$vcpkgManifestPath = Join-Path $root "vcpkg.json"
$licenseNotesPath = Join-Path $root "docs/licenses.md"
$releaseManifestPath = Join-Path $packagePath "release-manifest.json"

Assert-PathInsideRoot $packagePath
Assert-PathInsideRoot $outputPath
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

if (-not (Test-Path -LiteralPath $packagePath)) {
  throw "Package directory not found: $packagePath"
}

if (-not (Test-Path -LiteralPath $vcpkgManifestPath)) {
  throw "vcpkg manifest not found: $vcpkgManifestPath"
}

$vcpkgManifest = Get-Content -LiteralPath $vcpkgManifestPath -Raw | ConvertFrom-Json
$dependencies = @()

foreach ($dependency in $vcpkgManifest.dependencies) {
  if ($dependency -is [string]) {
    $dependencies += [ordered]@{
      name = $dependency
      features = @()
    }
  } else {
    $dependencies += [ordered]@{
      name = $dependency.name
      features = @($dependency.features)
    }
  }
}

$releaseVersion = $vcpkgManifest.'version-semver'
$releaseCommit = "unknown"

if (Test-Path -LiteralPath $releaseManifestPath) {
  $releaseManifest = Get-Content -LiteralPath $releaseManifestPath -Raw | ConvertFrom-Json
  $releaseVersion = $releaseManifest.version
  $releaseCommit = $releaseManifest.commit
}

$dependencyPackages = @()
foreach ($dependency in $dependencies) {
  $dependencyPackages += [ordered]@{
    SPDXID = New-PackageIdentifier -Name $dependency.name
    name = $dependency.name
    versionInfo = "NOASSERTION"
    downloadLocation = "NOASSERTION"
    filesAnalyzed = $false
    licenseConcluded = "NOASSERTION"
    licenseDeclared = "NOASSERTION"
    supplier = "NOASSERTION"
    externalRefs = @(
      [ordered]@{
        referenceCategory = "PACKAGE-MANAGER"
        referenceType = "purl"
        referenceLocator = "pkg:generic/$($dependency.name)"
      }
    )
    comment = if ($dependency.features.Count -gt 0) {
      "vcpkg dependency with features: $($dependency.features -join ', ')"
    } else {
      "vcpkg dependency"
    }
  }
}

$filePackages = @()
$runtimeFiles = Get-ChildItem -LiteralPath $packagePath -File -Recurse |
  Sort-Object -Property FullName

foreach ($file in $runtimeFiles) {
  $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
  $relativePath = ConvertTo-RepoRelativePath -Path $file.FullName

  $filePackages += [ordered]@{
    SPDXID = "SPDXRef-File-$($hash.Hash.ToLowerInvariant())"
    fileName = $relativePath
    checksums = @(
      [ordered]@{
        algorithm = "SHA256"
        checksumValue = $hash.Hash.ToLowerInvariant()
      }
    )
    licenseConcluded = "NOASSERTION"
    copyrightText = "NOASSERTION"
  }
}

$sbom = [ordered]@{
  spdxVersion = "SPDX-2.3"
  dataLicense = "CC0-1.0"
  SPDXID = "SPDXRef-DOCUMENT"
  name = "Uburu Windows release $releaseVersion"
  documentNamespace = "https://uburu.local/spdx/uburu-windows-$releaseVersion-$([Guid]::NewGuid())"
  creationInfo = [ordered]@{
    created = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
    creators = @("Tool: scripts/generate-release-sbom.ps1")
  }
  packages = @(
    [ordered]@{
      SPDXID = "SPDXRef-Package-Uburu"
      name = "Uburu"
      versionInfo = $releaseVersion
      downloadLocation = "NOASSERTION"
      filesAnalyzed = $false
      licenseConcluded = "LicenseRef-Uburu-Personal-Use"
      licenseDeclared = "LicenseRef-Uburu-Personal-Use"
      supplier = "Person: Dheovani Xavier da Cruz"
      comment = "Release commit: $releaseCommit"
    }
  ) + $dependencyPackages
  files = $filePackages
  hasExtractedLicensingInfos = @(
    [ordered]@{
      licenseId = "LicenseRef-Uburu-Personal-Use"
      extractedText = "See LICENSE in the release bundle."
      name = "Uburu Personal Use License"
    }
  )
}

$sbom | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $sbomPath -Encoding UTF8

$dependencyLines = foreach ($dependency in $dependencies) {
  if ($dependency.features.Count -gt 0) {
    "- $($dependency.name) with features: $($dependency.features -join ', ')"
  } else {
    "- $($dependency.name)"
  }
}

$licenseNotes = if (Test-Path -LiteralPath $licenseNotesPath) {
  Get-Content -LiteralPath $licenseNotesPath -Raw
} else {
  "docs/licenses.md was not found when this report was generated."
}

$licenseReport = @"
# Third-party notices

This report is generated for the Uburu Windows release bundle. It records the dependency manifest and points reviewers to the license notes that must be checked before public distribution.

## Release

- Version: $releaseVersion
- Commit: $releaseCommit
- Package directory: $(ConvertTo-RepoRelativePath -Path $packagePath)
- SBOM: $(ConvertTo-RepoRelativePath -Path $sbomPath)

## vcpkg dependencies

$($dependencyLines -join "`n")

## License review notes

$licenseNotes
"@

$licenseReport | Set-Content -LiteralPath $licenseReportPath -Encoding UTF8

Write-Output "SBOM: $sbomPath"
Write-Output "License report: $licenseReportPath"
