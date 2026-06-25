# Build

O fluxo preferido usa `CMakePresets.json`. Ele evita caminhos absolutos versionados e deixa cada
máquina informar seus próprios diretórios por variáveis de ambiente.

## Variáveis de ambiente

- `VCPKG_ROOT`: raiz do vcpkg.
- `QT_ROOT`: prefixo do Qt usado pelo CMake. Exemplo Windows/MinGW:
  `C:\Qt\6.11.1\mingw_64`.
- `MINGW_ROOT`: raiz do toolchain MinGW. Exemplo: `C:\Qt\Tools\mingw1310_64`.
- `NINJA_ROOT`: diretório que contém `ninja.exe`, quando Ninja não estiver no `PATH`. Exemplo:
  `C:\Qt\Tools\Ninja`.

`CMakeUserPresets.json` é ignorado pelo Git e pode ser usado para aliases ou caminhos pessoais.
`.env.example` lista as variáveis esperadas. Copie para `.env` se quiser registrar seus caminhos
locais. Os scripts PowerShell carregam `.env` automaticamente; os presets do CMake ainda exigem que
as variáveis estejam no ambiente do terminal.

Exemplo PowerShell:

```powershell
$env:VCPKG_ROOT = "C:\Users\dheov\vcpkg"
$env:QT_ROOT = "C:\Qt\6.11.1\mingw_64"
$env:MINGW_ROOT = "C:\Qt\Tools\mingw1310_64"
$env:NINJA_ROOT = "C:\Qt\Tools\Ninja"
```

## Windows com Qt/MinGW

Garanta que Ninja esteja no `PATH` e execute:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
```

Para executar a aplicação sem instalar DLLs no sistema:

```powershell
.\scripts\run-windows-mingw-desktop.ps1
```

Para criar uma pasta local redistribuível de desenvolvimento com `windeployqt` e as DLLs do vcpkg:

```powershell
.\scripts\deploy-windows-mingw-desktop.ps1
```

O script escreve por padrão em `dist/windows-mingw-debug`.

## Windows com MSVC

Use um Developer PowerShell/Prompt do Visual Studio, instale um Qt compatível com MSVC e configure
`QT_ROOT` para esse prefixo. Depois:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

## Linux

Com Ninja, vcpkg e Qt disponíveis:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Se o Qt estiver instalado fora dos prefixos padrão do sistema, defina `QT_ROOT` antes de configurar.

## Core sem Qt

O projeto permite `UBURU_BUILD_DESKTOP=OFF` para compilar e testar o core sem Qt:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-mingw-debug
```

No Linux, use `core-linux-debug` para o mesmo fluxo sem Qt.

## Presets de qualidade

O projeto separa presets de desenvolvimento e presets de gate. Os presets abaixo não exigem Qt e
são adequados para CI e validação rápida do core:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-mingw-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-mingw-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-mingw-werror-debug
```

No Linux, use `core-linux-werror-debug` para tratar warnings como erros. Para ASan/UBSan, use
`core-linux-sanitize-debug`.

As opções CMake correspondentes são:

- `UBURU_WARNINGS_AS_ERRORS`: promove warnings dos targets próprios para erro.
- `UBURU_ENABLE_SANITIZERS`: habilita AddressSanitizer e UndefinedBehaviorSanitizer onde suportado.

Sanitizers ficam desligados por padrão para não afetar builds normais, empacotamento ou ambientes
onde o runtime não esteja disponível.

## Política inicial de versões

- CMake mínimo: 3.25.
- Padrão C++: C++23, com `CMAKE_CXX_EXTENSIONS=OFF`.
- Qt mínimo: 6.5.
- Windows/MinGW validado: Qt 6.11.1 com MinGW 13.1.
- Windows/MSVC: suportado quando o preset é executado dentro de um ambiente de desenvolvimento do
  Visual Studio e com um Qt compatível em `QT_ROOT`.
- Linux: preset inicial com Ninja e triplet `x64-linux`.
- vcpkg: usado para Catch2, SQLite, PCRE2 e libgit2. O baseline está fixado em `vcpkg.json`.

## Build manual

Os comandos manuais continuam disponíveis quando um preset não se encaixar no ambiente:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Qt 6.5 ou superior deve estar em `CMAKE_PREFIX_PATH` ou `Qt6_DIR`.

## Formatação C++

Todo arquivo C++ próprio em `apps/`, `src/` e `tests/` deve respeitar o `.clang-format` da raiz.
Com `clang-format` disponível no `PATH` ou em uma instalação padrão do Visual Studio, o CMake
cria o target opcional `format`:

O build impõe C++23 por `CMAKE_CXX_STANDARD`. No `.clang-format`, `Standard: Latest` é usado porque
o formatador não oferece o valor `c++23`; essa opção habilita as regras de sintaxe C++ mais recentes
reconhecidas pela versão instalada.

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command tidy -Preset core-windows-mingw-debug
```

Também é possível formatar um arquivo isolado diretamente:

```powershell
clang-format -i src/core/search/direct-search-engine.cpp
```

O target não participa do build normal. Dependências vendorizadas, arquivos gerados e diretórios de
build não são formatados.

## Análise estática

O arquivo `.clang-tidy` define o conjunto inicial de checks. Quando `clang-tidy` está disponível, o
CMake cria o target opcional `tidy`:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-mingw-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command tidy -Preset core-windows-mingw-debug
```

Esse target é diagnóstico e não participa do build normal. A CI futura deve decidir quando transformar
diagnósticos em bloqueios.

Observação: no ambiente Windows/MinGW validado em desenvolvimento, o `clang-tidy` distribuído com
Visual Studio 18 foi encontrado, mas crashou ao analisar o compile database MinGW. Por isso, o alvo
está versionado como preparação, mas ainda não é critério concluído do Marco 0 até validarmos uma
versão estável da ferramenta ou outro backend de análise.

## CI

O workflow `.github/workflows/ci.yml` valida inicialmente o core independente de Qt:

- Windows/MinGW com warnings como erros;
- Linux com warnings como erros;
- Linux com AddressSanitizer e UndefinedBehaviorSanitizer;
- testes via CTest;
- `format-check` sem modificar arquivos.

A aplicação desktop Qt continua validada localmente pelo preset `windows-mingw-debug`. Um job Qt
completo deve ser adicionado quando o pipeline de instalação/cache do Qt estiver estabilizado.
