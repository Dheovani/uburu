# Contribuindo

Obrigado por considerar contribuir com o Uburu. O projeto ainda está em fase de fundação, então a
prioridade é preservar arquitetura, correção e reprodutibilidade.

## Fluxo recomendado

1. Abra ou escolha um item do `TODO.md`.
2. Crie uma branch curta a partir de `main`.
3. Mantenha a mudança pequena e revisável.
4. Atualize documentação quando comportamento, arquitetura ou comandos mudarem.
5. Rode build, testes e `format-check` antes de abrir uma revisão.

## Comandos locais mínimos

No Windows/MinGW validado:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check
```

Para trabalhar sem Qt, use o preset `core-windows-mingw-debug`.

## Estilo

- Código C++ deve seguir o `.clang-format` da raiz.
- Arquivos próprios novos devem usar `kebab-case`, salvo nomes canônicos de ferramentas.
- Não introduza texto visível ao usuário sem passar por i18n.
- Não acople lógica de busca ao QML.
- Evite números mágicos; prefira constantes nomeadas com escopo mínimo.
- Prefira código direto, mas preserve clareza e equivalência semântica.

## Commits

Use o formato documentado em `docs/development.md`:

```txt
tipo(escopo): resumo imperativo curto
```

Exemplo:

```txt
build: add core ci quality gates
```
