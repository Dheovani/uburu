# Desenvolvimento

Este documento registra as regras operacionais do repositório. Ele complementa o `AGENTS.md`, que
define a visão técnica e as normas de implementação.

## Branch principal

A branch principal do projeto é `main`.

Trabalhos maiores devem acontecer em branches curtas e descritivas, preferencialmente com o prefixo
da área afetada:

- `build/...`
- `search/...`
- `ui/...`
- `docs/...`
- `git/...`

## Política de commits

Use commits pequenos, revisáveis e orientados a uma intenção clara. A mensagem deve seguir este
formato:

```txt
tipo(escopo): resumo imperativo curto
```

Exemplos:

```txt
feat(search): add deterministic direct search ordering
build: add reproducible CMake presets
docs: document direct search semantics
```

Tipos recomendados:

- `feat`: comportamento novo.
- `fix`: correção de bug.
- `test`: testes.
- `docs`: documentação.
- `build`: CMake, vcpkg, scripts, CI e tooling.
- `refactor`: mudança interna sem comportamento novo.
- `chore`: manutenção sem impacto funcional direto.

## Antes de commitar

Para o fluxo Windows/MSVC validado:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format-check
```

Quando a alteração tocar código C++, rode `format` antes do `format-check`:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command format
```

Para validar o core com a mesma régua inicial de CI:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command configure -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command build -Preset core-windows-msvc-werror-debug
powershell -ExecutionPolicy Bypass -File .\scripts\invoke-cmake-preset.ps1 -Command test -Preset core-windows-msvc-werror-debug
```

No Linux, rode também `core-linux-sanitize-debug` antes de mudanças sensíveis em parsing, regex,
filesystem, concorrência ou armazenamento.

## O que não commitar

Não commite:

- diretórios `build/`, `build-*`, `dist/` ou `out/`;
- `.env`;
- `CMakeUserPresets.json`;
- bancos locais, logs ou artefatos gerados;
- dependências vendorizadas sem decisão explícita.

## Relação com o TODO

O `TODO.md` é o plano operacional do projeto. Quando uma mudança concluir um item do TODO, marque o
item na mesma alteração e valide conforme o critério descrito no próprio arquivo.
