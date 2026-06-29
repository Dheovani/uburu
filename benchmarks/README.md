# Benchmarks

Este diretório contém benchmarks reproduzíveis de desenvolvimento. Eles ficam fora do build padrão para
não atrasar compilação, testes e CI.

## Storage FTS5

`uburu-storage-fts5-benchmark` compara uma consulta textual simples no catálogo SQLite por `LIKE` contra
uma consulta equivalente em FTS5. O objetivo é avaliar FTS5 como estrutura auxiliar para busca indexada,
sem acoplar o contrato do `StorageService` a esse backend.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 `
  -Command configure `
  -Preset core-windows-mingw-benchmarks-debug

powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 `
  -Command build `
  -Preset core-windows-mingw-benchmarks-debug

. .\scripts\load-local-env.ps1
$benchmarkRuntime = Join-Path (Get-Location) "build\core-windows-mingw-benchmarks-debug\vcpkg_installed\x64-mingw-dynamic\debug\bin"
$mingwRuntime = Join-Path $env:MINGW_ROOT "bin"
$env:Path = "$benchmarkRuntime;$mingwRuntime;$env:Path"

.\build\core-windows-mingw-benchmarks-debug\benchmarks\uburu-storage-fts5-benchmark.exe
```

Cada benchmark deve registrar dataset, hardware, configuração, orçamento de memória e resultado observado
antes de orientar uma decisão arquitetural.
