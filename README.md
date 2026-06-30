# Uburu

Uburu é uma aplicação desktop de busca avançada em arquivos e repositórios de software. A base atual oferece uma busca literal direta e progressiva, uma UI Qt Quick não bloqueante e contratos para a futura indexação persistente e consciente de Git.

O planejamento completo de evolução e os critérios para a versão 1.0 estão em [TODO.md](TODO.md).
Regras de branch, commits e validação local ficam em [docs/development.md](docs/development.md).

## Dependências

- CMake 3.25+
- compilador com suporte a C++23
- Visual Studio 2026/MSVC no Windows; Ninja continua suportado para fluxos específicos
- Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Concurrent`, `LinguistTools`)
- Catch2 3 para os testes
- SQLite, PCRE2 e libgit2 são detectados opcionalmente pelo core nesta etapa

O manifesto `vcpkg.json` fornece as dependências não Qt. O Qt pode ser instalado pelo gerenciador preferido do sistema ou por uma feature de overlay do vcpkg.

## Política inicial de versões

- CMake mínimo: 3.25, por causa de `CMakePresets.json` e recursos modernos de build.
- C++: C++23 sem extensões de compilador.
- Qt mínimo: 6.5.
- Windows/MSVC validado: Visual Studio 18 2026 com Qt 6.11.1 `msvc2022_64`.
- vcpkg: obrigatório para dependências não Qt; o baseline está fixado em `vcpkg.json`.

## Build recomendado por presets

Configure estas variáveis de ambiente:

- `VCPKG_ROOT`: raiz do vcpkg.
- `QT_ROOT`: prefixo do Qt usado pelo CMake, por exemplo `C:\Qt\6.11.1\msvc2022_64`.

Use `.env.example` como referência para criar um `.env` local. O arquivo `.env` é ignorado pelo Git.
Os scripts PowerShell em `scripts/` carregam `.env` automaticamente; os presets do CMake ainda leem
variáveis do ambiente do processo, então defina-as no terminal antes de executar `cmake --preset ...`.

No Windows com Qt/MSVC:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
.\scripts\run-windows-msvc-desktop.ps1
.\scripts\deploy-windows-msvc-desktop.ps1
```

Para trabalhar apenas no core sem uma instalação do Qt:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-debug
```

No Windows, o fluxo local principal usa MSVC e Qt.

## Build manual no Windows com PowerShell

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Confirme a configuração do vcpkg com `Write-Output $env:VCPKG_ROOT`. No PowerShell, use sempre
`$env:VCPKG_ROOT`; a forma `$VCPKG_ROOT` pertence a shells POSIX.

## Build no Linux ou macOS

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

O preset local `local-windows-msvc-debug` usa o gerador do Visual Studio e não exige Developer Prompt.

## Formatação

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command tidy
```

## Qualidade e CI

Os gates iniciais do core usam presets sem Qt:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-werror-debug
```

No Linux, os presets equivalentes são `core-linux-werror-debug` e
`core-linux-sanitize-debug`. O workflow em `.github/workflows/ci.yml` executa configure, build,
testes, sanitizers e `format-check` para o core.

Consulte também:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [SECURITY.md](SECURITY.md)
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- [docs/licenses.md](docs/licenses.md)

## Estado atual

A busca literal lê arquivos linha a linha, respeita limite de tamanho, extensão, arquivos ocultos, cancelamento e limite de resultados. Regex, `.gitignore`, detecção completa de encoding/binários, storage SQLite, índice e backend libgit2 estão explicitamente reservados pelos contratos e pela documentação; a UI informa quando regex ainda não está disponível.
