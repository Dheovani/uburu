param(
  [string]$EnvFile = (Join-Path (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")) ".env")
)

if (-not (Test-Path -LiteralPath $EnvFile)) {
  return
}

foreach ($line in Get-Content -LiteralPath $EnvFile) {
  $trimmed = $line.Trim()

  if ($trimmed.Length -eq 0 -or $trimmed.StartsWith("#")) {
    continue
  }

  if ($trimmed -notmatch "^([A-Za-z_][A-Za-z0-9_]*)=(.*)$") {
    continue
  }

  $name = $Matches[1]
  $value = $Matches[2].Trim()

  if (($value.StartsWith('"') -and $value.EndsWith('"')) -or
      ($value.StartsWith("'") -and $value.EndsWith("'"))) {
    $value = $value.Substring(1, $value.Length - 2)
  }

  Set-Item -Path "Env:$name" -Value $value
}
