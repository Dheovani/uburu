# Indexação

O catálogo persistente será dividido entre metadados mutáveis da worktree e documentos endereçados por conteúdo.

Uma entrada lógica contém `repositoryId`, `worktreeId`, caminho relativo, tamanho, mtime, hash de conteúdo, blob Git opcional e status local. O documento de conteúdo pode ser reutilizado quando o mesmo hash reaparece em outra branch ou worktree.

## Formato interno versionado

Todo documento indexado possui `formatVersion`. A versão atual é `1` e representa um documento
endereçado por conteúdo com hash SHA-256, metadados de blob Git opcional e suporte a overlay da working
tree. Essa versão é persistida no SQLite junto ao documento e ao caminho que aponta para ele.

O módulo `core/index` é o dono da interpretação desse formato. `core/storage` apenas persiste a versão
para que upgrades futuros possam decidir se um documento pode ser reutilizado, migrado ou descartado. O
formato do documento é separado da versão do schema SQLite: uma migration de schema pode existir sem
mudar a semântica do documento indexado, e uma versão nova de documento pode exigir reindexação mesmo
que o schema continue compatível.

## Atualização incremental

1. Capturar HEAD, branch e estado do index Git.
2. Consumir eventos do watcher e reconciliar com um scan periódico.
3. Calcular hash somente quando tamanho/mtime ou estado Git indicarem mudança.
4. Reutilizar documentos por blob hash ou hash de conteúdo.
5. Aplicar o overlay da working tree para adicionados, modificados e deletados.
6. Publicar uma nova geração do índice de forma atômica.

Uma troca de branch invalida o catálogo visível, não o armazenamento endereçado por conteúdo. Backpressure e orçamento de memória devem limitar parsing e filas de resultados.
