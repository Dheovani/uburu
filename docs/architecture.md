# Arquitetura

## Direção de dependências

```text
QML -> SearchController -> SearchEngine
                           |-> FileScanner
                           |-> Text matcher

IndexService -> GitService / StorageService / FileScanner
```

`uburu_core` é uma biblioteca C++23 sem dependência de Qt. A aplicação desktop conhece o core por interfaces e converte resultados em um `QAbstractListModel`; QML nunca executa busca nem acessa arquivos diretamente.

Os tipos compartilhados ficam em `src/shared/types`. Eles modelam identidade lógica de repositório e worktree, caminho relativo e identidade de conteúdo separadamente. Isso evita transformar caminho em identidade de documento.

## Concorrência

O `SearchController` agenda a busca no pool de `QtConcurrent`. Resultados são devolvidos progressivamente à thread da UI por eventos enfileirados. O core usa `std::stop_token`, permitindo que CLI, testes ou outras interfaces usem o mesmo cancelamento sem Qt.

Os próximos serviços concretos devem ser construídos atrás das interfaces existentes. Operações de filesystem, Git ou SQLite não devem migrar para o controller.
