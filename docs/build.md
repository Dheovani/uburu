# Build

Requisitos e comandos estão no `README.md`. O projeto permite `UBURU_BUILD_DESKTOP=OFF` para compilar e testar o core sem Qt. `UBURU_ENABLE_PCRE2` e `UBURU_ENABLE_LIBGIT2` controlam a descoberta opcional dos backends.

No Windows com vcpkg:

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
cmake --build build --target format
```

Também é possível formatar um arquivo isolado diretamente:

```powershell
clang-format -i src/core/search/direct-search-engine.cpp
```

O target não participa do build normal. Dependências vendorizadas, arquivos gerados e diretórios de
build não são formatados.
