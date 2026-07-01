# Storage

SQLite armazena catálogos de repositórios, worktrees, caminhos e gerações do índice, além de
preferências, histórico de buscas, buscas salvas e métricas operacionais. Conteúdo indexado é
referenciado por hash; caminhos apontam para documentos e não constituem sua identidade.

O backend usa transações curtas, WAL, migrations versionadas e prepared statements. Escritas de uma nova
geração são atômicas. FTS5 pode acelerar partes da consulta no futuro, mas deve permanecer atrás de uma
interface substituível e não será a única representação do índice.

## Backend inicial

`SQLiteStorageService` é o primeiro backend concreto de `StorageService`. Ele mantém o cabeçalho livre de
`sqlite3.h`, abre a conexão por RAII, fecha a conexão no destrutor e falha explicitamente quando usado
antes de `initialize()`. A implementação usa prepared statements para escrita e leitura, além de
transações curtas nas operações que precisam atualizar mais de uma tabela.

Na inicialização, o backend configura e expõe por `pragmaSnapshot()`:

- `PRAGMA foreign_keys = ON`;
- `PRAGMA journal_mode = WAL`;
- `PRAGMA synchronous = NORMAL`;
- `sqlite3_busy_timeout` com timeout inicial de 5 segundos;
- `PRAGMA user_version = 7`;
- tabela `schema_migrations` para registrar versões aplicadas.

Esses pragmas são parte do contrato operacional do storage. Testes automatizados verificam que o banco
abre com foreign keys habilitadas, WAL ativo, busy timeout configurado e integridade reportada como
válida.

## Localização do banco e migração

`defaultStorageDatabasePath()` escolhe uma localização padrão por plataforma:

- Windows: `%LOCALAPPDATA%/uburu/uburu.db`, com fallback para `%APPDATA%`, `%USERPROFILE%` e diretório
  temporário;
- macOS: `$HOME/Library/Application Support/uburu/uburu.db`;
- Linux e outros Unix: `$XDG_DATA_HOME/uburu/uburu.db` ou `$HOME/.local/share/uburu/uburu.db`.

`migrateStorageDatabase()` copia o banco de origem para o destino e preserva os sidecars `-wal` e `-shm`
quando existirem. A migração é uma cópia segura: cria o diretório de destino, sobrescreve o destino
quando solicitado pela aplicação e não apaga o banco de origem. A decisão de remover ou arquivar o banco
antigo pertence à camada de aplicação/configuração, não ao helper de baixo nível.

## Schema

O schema evolui por migrations idempotentes:

1. `repositories`, `worktrees`, `generations`, `documents`, `files` e `overlays`;
2. algoritmos explícitos para hash de conteúdo e blob Git;
3. identidade persistente de documento por `(content_hash_algorithm, content_hash)`;
4. metadados de produto: `preferences`, `search_history`, `saved_searches` e `indexing_metrics`.
5. versão explícita do formato interno de documentos indexados em `documents` e `files`.
6. `mtime` persistido em `files` para permitir reuso incremental conservador por catálogo.
7. texto indexado normalizado em `documents.indexed_text` para busca de conteúdo sem reler arquivos.

Tabelas principais:

- `schema_migrations`: versões de migration já aplicadas;
- `repositories`: repositórios Git lógicos;
- `worktrees`: worktrees físicas, incluindo estados `locked`, `prunable` e motivo de lock;
- `generations`: gerações do índice separadas por repositório, worktree, HEAD e branch;
- `documents`: documentos endereçados por algoritmo e hash de conteúdo;
- `files`: associação entre caminho relativo na worktree e documento indexado;
- `overlays`: contrato persistente para overlay da working tree sobre conteúdo versionado;
- `preferences`: preferências globais e por repositório;
- `search_history`: histórico recente de buscas com retenção limitada;
- `saved_searches`: buscas nomeadas pelo usuário;
- `indexing_metrics`: métricas recentes de indexação com retenção limitada.

Mesmo nesta etapa, caminho e conteúdo não são a mesma identidade: `files` aponta para `documents` por
`content_hash`. O mesmo documento poderá ser reutilizado por múltiplos caminhos, worktrees ou branches
quando o indexador começar a preencher gerações reais.

`documents` e `files` armazenam o algoritmo junto com o valor do hash. Para conteúdo próprio do Uburu, o
domínio usa `ContentHashAlgorithm`; para blob Git, usa `GitObjectHashAlgorithm`. A chave primária de
`documents` inclui o algoritmo e o valor do hash, evitando colisões semânticas entre algoritmos
diferentes que produzam a mesma representação textual.

`documents` e `files` também armazenam `format_version`. Essa versão descreve o formato interno do
documento indexado, não a versão do schema SQLite. Ela permite que versões futuras do Uburu migrem,
reutilizem ou descartem documentos de cache com segurança sem depender apenas da estrutura das tabelas.

`documents.indexed_text` armazena a primeira representação textual persistida do conteúdo. O campo é
endereçado pelo mesmo hash do documento, então múltiplos caminhos, branches ou worktrees podem reutilizar
o mesmo texto indexado quando apontarem para conteúdo idêntico. A representação inicial é texto
normalizado por linha e ainda não substitui tokenização, FTS ou compactação futuras.

`files` armazena o `mtime` como ticks nativos de `std::filesystem::file_time_type`. Esse valor não é
tratado como data de usuário nem como timestamp Unix; ele existe para round-trip local e comparação
incremental entre o catálogo persistido e o próximo `FileEntry` observado pela varredura.

## Publicação de gerações

`StorageService::publishGeneration()` publica uma visão completa da worktree em uma transação
`BEGIN IMMEDIATE`. A operação:

1. cria uma linha em `generations` ainda não publicada;
2. remove a visão anterior de `files` para a worktree;
3. insere ou reutiliza documentos por `content_hash`;
4. recria os caminhos da geração em `files`;
5. marca a geração como publicada;
6. confirma a transação.

Se qualquer documento pertencer a outro repositório ou worktree, ou se qualquer escrita falhar, a
transação é revertida. Assim, o índice nunca deve observar metade da geração nova e metade da anterior.
Testes cobrem publicação atômica, rollback de geração inválida e consistência entre conexão leitora e
conexão escritora.

## Recuperação, integridade e reconstrução

As migrations rodam dentro de transações explícitas. Se uma migration falhar, a transação é revertida e
o banco não deve avançar `user_version`. Durante `initialize()`, o backend também remove registros de
`generations` que ficaram com `published = 0`, representando publicações interrompidas antes de se
tornarem visíveis.

`StorageService::recoverIncompleteGenerations()` expõe a mesma limpeza para chamadas controladas pelo
indexador. O método remove somente metadados de gerações não publicadas; ele não remove documentos nem a
visão publicada anterior.

`StorageService::validateIntegrity()` executa `PRAGMA integrity_check` e retorna um relatório explícito.
Quando a estrutura do índice precisar ser descartada sem apagar metadados de produto,
`StorageService::rebuildIndexCatalog()` remove `overlays`, `files`, `documents` e `generations` em uma
transação. Essa reconstrução preserva repositórios, worktrees, preferências, histórico, buscas salvas e
métricas.

`StorageService::collectOrphanDocuments()` remove documentos que não são mais referenciados por nenhum
caminho em `files`, usando a identidade composta `(content_hash_algorithm, content_hash)`. Essa coleta é
separada da publicação de geração para permitir políticas futuras de retenção, orçamento de disco e
diagnóstico antes de apagar cache reaproveitável.

`StorageService::enforceDocumentBudget()` aplica a primeira política de orçamento de disco do índice. A
política é conservadora: calcula o total de bytes armazenados em `documents` e remove apenas documentos
órfãos, começando pelos mais antigos, até o total ficar dentro do limite solicitado. Documentos ainda
referenciados por `files` nunca são removidos por essa rotina. Se o orçamento continuar excedido porque
todo o conteúdo restante ainda está vivo, o método retorna `budgetExceeded = true` para que uma camada de
aplicação decida entre reconfigurar o limite, pedir uma limpeza explícita ou reconstruir o índice.

## Preferências, histórico e métricas

Preferências usam uma chave textual e um escopo:

- escopo vazio: preferência global;
- `RepositoryId`: preferência específica de repositório.

Histórico de buscas e métricas de indexação recebem um limite de retenção no momento da escrita. O
backend insere o novo registro e remove os mais antigos que excedam o orçamento, evitando crescimento
ilimitado. Buscas salvas são identificadas por nome e podem ser atualizadas sem duplicar registros.

Essas tabelas são deliberadamente simples nesta etapa. A validação semântica de chaves, tipos de
preferência, privacidade do histórico e políticas de exportação/importação pertencem a marcos futuros de
configuração e UX.

## Avaliação inicial de FTS5

O Marco 5 introduziu `uburu-storage-fts5-benchmark`, um benchmark de desenvolvedor desligado do build
padrão. Ele cria um dataset determinístico em SQLite, compara uma consulta textual simples por `LIKE`
contra uma consulta equivalente por FTS5 e valida que ambas retornam a mesma contagem.

Resultado local inicial no preset `core-windows-msvc-debug`:

- documentos: 20.000;
- documentos correspondentes: 2.858;
- repetições por estratégia: 30;
- `LIKE`: 228.924 µs;
- FTS5: 9.993 µs.

Essa medição justifica manter FTS5 como candidato forte para aceleração de consultas textuais indexadas,
mas não muda o contrato do storage: o índice persistente continua content-addressed e Git-aware, e FTS5
deve ser tratado como backend/estrutura auxiliar substituível.

## Escopo desta etapa

Esta etapa persiste e recupera `RepositoryInfo`, `WorktreeInfo` e `IndexDocument`, incluindo remoção
lógica por caminho, publicação atômica de gerações, recuperação de publicações interrompidas, coleta de
documentos órfãos, pragmas medidos, relatório de integridade, reconstrução segura do catálogo,
preferências, histórico, buscas salvas, métricas recentes, localização/migração básica do banco e
avaliação inicial de FTS5 por benchmark.
