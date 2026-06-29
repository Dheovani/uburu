# Storage

SQLite armazenará catálogos de repositórios, worktrees, caminhos e gerações do índice, além de preferências, histórico e métricas. Conteúdo indexado será referenciado por hash; caminhos apontam para documentos e não constituem sua identidade.

O backend deverá usar transações curtas, WAL, migrações versionadas e prepared statements. Escritas de uma nova geração serão atômicas. FTS5 pode acelerar partes da consulta, mas ficará atrás de uma interface substituível e não será a única representação do índice.

## Backend inicial

`SQLiteStorageService` é o primeiro backend concreto de `StorageService`. Ele mantém o cabeçalho livre de
`sqlite3.h`, abre a conexão por RAII, fecha a conexão no destrutor e falha explicitamente quando usado
antes de `initialize()`. A implementação usa prepared statements para escrita e leitura, além de
transações curtas nas operações que precisam atualizar mais de uma tabela.

Na inicialização, o backend configura:

- `PRAGMA foreign_keys = ON`;
- `PRAGMA journal_mode = WAL`;
- `PRAGMA synchronous = NORMAL`;
- `sqlite3_busy_timeout` com timeout inicial de 5 segundos;
- `PRAGMA user_version = 3`;
- tabela `schema_migrations` para registrar versões aplicadas.

## Schema

O schema inicial cria as tabelas abaixo e evolui por migrations idempotentes. A versão 1 criou a base
do catálogo; a versão 2 adicionou algoritmos explícitos para hash de conteúdo e blob Git; a versão 3
tornou `(content_hash_algorithm, content_hash)` a identidade persistente do documento.

- `schema_migrations`: versões de migration já aplicadas;
- `repositories`: repositórios Git lógicos;
- `worktrees`: worktrees físicas, incluindo estados `locked`, `prunable` e motivo de lock;
- `generations`: gerações futuras do índice, já separadas por repositório/worktree/HEAD/branch;
- `documents`: documentos endereçados por `content_hash`;
- `files`: associação entre caminho relativo na worktree e documento indexado;
- `overlays`: contrato persistente para overlay da working tree sobre conteúdo versionado.

Mesmo nesta primeira versão, caminho e conteúdo não são a mesma identidade: `files` aponta para
`documents` por `content_hash`. O mesmo documento poderá ser reutilizado por múltiplos caminhos,
worktrees ou branches quando o indexador começar a preencher gerações reais.

`documents` e `files` armazenam o algoritmo junto com o valor do hash. Para conteúdo próprio do Uburu,
o domínio usa `ContentHashAlgorithm`; para blob Git, usa `GitObjectHashAlgorithm`. A chave primária de
`documents` inclui o algoritmo e o valor do hash, evitando colisões semânticas entre algoritmos
diferentes que produzam a mesma representação textual.

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

## Escopo desta etapa

Esta etapa persiste e recupera `RepositoryInfo`, `WorktreeInfo` e `IndexDocument`, incluindo remoção
lógica por caminho e publicação atômica de gerações. Ainda não implementa recuperação de banco
corrompido, retenção de documentos órfãos, preferências, histórico de buscas ou métricas persistentes.
Esses pontos permanecem no Marco 5.
