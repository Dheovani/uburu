# Uburu

Uburu é uma aplicação desktop de busca avançada em arquivos e repositórios de software. A base atual oferece uma busca literal direta e progressiva, uma UI Qt Quick não bloqueante e contratos para a futura indexação persistente e consciente de Git.

## Dependências

- CMake 3.25+
- compilador com suporte a C++23
- Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Concurrent`, `LinguistTools`)
- Catch2 3 para os testes
- SQLite, PCRE2 e libgit2 são detectados opcionalmente pelo core nesta etapa

O manifesto `vcpkg.json` fornece as dependências não Qt. O Qt pode ser instalado pelo gerenciador preferido do sistema ou por uma feature de overlay do vcpkg.

## Build

```sh
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Para trabalhar apenas no core sem uma instalação do Qt:

```sh
cmake -S . -B build-core -DUBURU_BUILD_DESKTOP=OFF
```

## Estado atual

A busca literal lê arquivos linha a linha, respeita limite de tamanho, extensão, arquivos ocultos, cancelamento e limite de resultados. Regex, `.gitignore`, detecção completa de encoding/binários, storage SQLite, índice e backend libgit2 estão explicitamente reservados pelos contratos e pela documentação; a UI informa quando regex ainda não está disponível.
