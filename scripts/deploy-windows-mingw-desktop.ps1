param()

$ErrorActionPreference = "Stop"

Write-Warning "MinGW is no longer the supported local Windows toolchain. Forwarding to MSVC."

& (Join-Path $PSScriptRoot "deploy-windows-msvc-desktop.ps1")
