# Uburu

Uburu é uma aplicação desktop de busca avançada em arquivos e repositórios de software. A base atual oferece uma busca literal direta e progressiva, uma UI Qt Quick não bloqueante e contratos para a futura indexação persistente e consciente de Git.

O planejamento completo de evolução e os critérios para a versão 1.0 estão em [TODO.md](TODO.md).
Regras de branch, commits e validação local ficam em [docs/development.md](docs/development.md).

## Dependências

- CMake 3.25+
- compilador com suporte a C++23
- Ninja para os presets versionados
- Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Concurrent`, `LinguistTools`)
- Catch2 3 para os testes
- SQLite, PCRE2 e libgit2 são detectados opcionalmente pelo core nesta etapa

O manifesto `vcpkg.json` fornece as dependências não Qt. O Qt pode ser instalado pelo gerenciador preferido do sistema ou por uma feature de overlay do vcpkg.

## Política inicial de versões

- CMake mínimo: 3.25, por causa de `CMakePresets.json` e recursos modernos de build.
- C++: C++23 sem extensões de compilador.
- Qt mínimo: 6.5.
- Windows/MinGW validado: Qt 6.11.1 com MinGW 13.1.
- Windows/MSVC: suportado por preset, desde que o ambiente do Visual Studio e um Qt compatível com
  MSVC estejam configurados.
- vcpkg: obrigatório para dependências não Qt; o baseline está fixado em `vcpkg.json`.

## Build recomendado por presets

Configure estas variáveis de ambiente:

- `VCPKG_ROOT`: raiz do vcpkg.
- `QT_ROOT`: prefixo do Qt usado pelo CMake, por exemplo `C:\Qt\6.11.1\mingw_64`.
- `MINGW_ROOT`: raiz do toolchain MinGW, por exemplo `C:\Qt\Tools\mingw1310_64`.
- `NINJA_ROOT`: diretório que contém `ninja.exe`, quando Ninja não estiver no `PATH`, por exemplo
  `C:\Qt\Tools\Ninja`.

Use `.env.example` como referência para criar um `.env` local. O arquivo `.env` é ignorado pelo Git.
Os scripts PowerShell em `scripts/` carregam `.env` automaticamente; os presets do CMake ainda leem
variáveis do ambiente do processo, então defina-as no terminal antes de executar `cmake --preset ...`.

No Windows com Qt/MinGW:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
.\scripts\run-windows-mingw-desktop.ps1
.\scripts\deploy-windows-mingw-desktop.ps1
```

Para trabalhar apenas no core sem uma instalação do Qt:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-mingw-debug
```

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

O preset `windows-msvc-debug` deve ser executado em um Developer PowerShell/Prompt do Visual Studio
com `QT_ROOT` apontando para uma instalação de Qt compatível com MSVC.

## Formatação

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command tidy -Preset core-windows-mingw-debug
```

## Qualidade e CI

Os gates iniciais do core usam presets sem Qt:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-mingw-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-mingw-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-mingw-werror-debug
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
