# Arquitetura

## Direção de dependências

```text
QML -> SearchController -> SearchEngine
                           |-> FileScanner
                           |-> TextFileReader
                           |-> Text matcher

IndexService -> GitService / StorageService / FileScanner
```

`uburu_core` é uma biblioteca C++23 sem dependência de Qt. A aplicação desktop conhece o core por interfaces e converte resultados em um `QAbstractListModel`; QML nunca executa busca nem acessa arquivos diretamente.

Os tipos compartilhados ficam em `src/shared/types`. Eles modelam identidade lógica de repositório e worktree, caminho relativo e identidade de conteúdo separadamente. Isso evita transformar caminho em identidade de documento.

## Concorrência

O `SearchController` agenda a busca no pool de `QtConcurrent`. Resultados são devolvidos progressivamente à thread da UI por eventos enfileirados. O core usa `std::stop_token`, permitindo que CLI, testes ou outras interfaces usem o mesmo cancelamento sem Qt.

Os próximos serviços concretos devem ser construídos atrás das interfaces existentes. Operações de filesystem, Git ou SQLite não devem migrar para o controller.

## Leitura de texto

`core/text` possui um leitor de arquivos orientado a linhas que entrega texto UTF-8 ao motor de
busca. O `SearchEngine` não interpreta BOM, UTF-16, Latin-1, binários ou finais de linha diretamente;
ele consome `TextLine` e delega matching para `text-matcher` ou `regex-matcher`.

Essa separação mantém encoding, política de binários, limites de linha e contexto de preview fora da
lógica de busca, preservando a possibilidade de evoluir para leitores mais especializados sem mudar
o contrato público do engine.

## Filesystem e ignore

`core/filesystem` contém o scanner recursivo e as regras de ignore. O scanner aplica filtros antes de
entregar `FileEntry` ao motor de busca, mantendo o `SearchEngine` livre de detalhes de diretório,
arquivos ocultos, globs e `.gitignore`.

As regras de `.gitignore` são carregadas por diretório. Arquivos `.gitignore` em subdiretórios
acrescentam regras com maior precedência para aquela subárvore. A implementação inicial cobre
comentários, padrões por basename, padrões com caminho, regras ancoradas, diretórios, negação e
desativação por `SearchOptions::respect_gitignore`.

Ignores globais do Git e `.git/info/exclude` ainda devem entrar por adaptadores explícitos para não
misturar descoberta Git com varredura genérica de filesystem.
