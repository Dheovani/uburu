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

Normalização de caminhos fica centralizada em `path-normalization`. Caminhos relativos são
convertidos para representação genérica com `/`, segmentos léxicos redundantes são removidos e
caminhos absolutos são rejeitados quando uma API exige caminho relativo. Chaves de comparação seguem
a plataforma: no Windows, comparações ASCII de caminhos são case-insensitive; em plataformas POSIX,
mantêm sensibilidade a maiúsculas e minúsculas. Isso mantém filtros, globs, ordenação determinística
e regras de ignore usando a mesma semântica.

No Windows, a normalização não impõe limites do legado `MAX_PATH` e preserva prefixos UNC e
extended-length (`\\servidor\share` e `\\?\...`) como parte da chave normalizada. Backends nativos
futuros ainda podem precisar de adaptação específica para APIs Win32, mas o core não deve truncar,
descartar ou comparar esses caminhos como strings comuns sensíveis a caixa.

O scanner detecta arquivos esparsos quando a plataforma expõe essa informação. No Windows, a
detecção usa `FILE_ATTRIBUTE_SPARSE_FILE`; em plataformas POSIX compatíveis, compara blocos alocados
com tamanho lógico reportado por `stat`. A política inicial é conservadora: `FileEntry::sparse`
apenas sinaliza o atributo e a busca direta ainda aplica os mesmos limites de tamanho e leitura dos
demais arquivos. Otimizações ou restrições específicas para sparse files devem ser adicionadas
somente com benchmarks e sem perder resultados por padrão.

Diretórios symlink, junctions e reparse points são tratados como fronteiras explícitas de
travessia. Por padrão, o scanner não entra nesses diretórios. Quando `SearchOptions::follow_symlinks`
está ativo, o scanner pode atravessá-los, mas registra identidades canônicas dos diretórios visitados
para evitar ciclos. Mount points são tratados como diretórios normais nesta fase; uma política futura
para bloquear cruzamento de volumes deve ser adicionada como opção explícita, não como efeito
colateral da varredura.

Watchers de filesystem usam a interface `FileWatcher`, que emite eventos relativos de criação,
modificação e remoção. O backend inicial é `PollingFileWatcher`: ele mantém snapshots normalizados e
compara tamanho, timestamp e tipo da entrada a cada `poll()`. Esse fallback é portátil e simples,
mas não substitui backends nativos eficientes. Implementações futuras com `ReadDirectoryChangesW`,
`inotify` e `FSEvents` devem preservar o mesmo contrato e sinalizar perda/overflow de eventos para
permitir rescan de reconciliação.

As regras de `.gitignore` são carregadas por diretório. Arquivos `.gitignore` em subdiretórios
acrescentam regras com maior precedência para aquela subárvore. A implementação inicial cobre
comentários, padrões por basename, padrões com caminho, regras ancoradas, diretórios, negação e
desativação por `SearchOptions::respect_gitignore`.

O scanner também carrega `.git/info/exclude` a partir da raiz pesquisada e arquivos globais de
ignore passados explicitamente em `SearchOptions::global_git_ignore_files`. A descoberta automática
do caminho global configurado no Git fica fora do scanner e deve entrar pelo `GitService` ou pela
camada de configuração, para não misturar leitura de configuração Git com varredura genérica de
filesystem.
