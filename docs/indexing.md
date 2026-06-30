# Indexação

O catálogo persistente é dividido entre metadados mutáveis da worktree e documentos endereçados por
conteúdo.

Uma entrada lógica contém `repositoryId`, `worktreeId`, caminho relativo, tamanho, mtime, hash de
conteúdo, blob Git opcional e status local. O documento de conteúdo pode ser reutilizado quando o mesmo
hash reaparece em outra branch ou worktree.

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

Uma troca de branch invalida o catálogo visível, não o armazenamento endereçado por conteúdo.
Backpressure e orçamento de memória devem limitar parsing e filas de resultados.

A primeira base incremental compara a entrada persistida do catálogo com o `FileEntry` atual. Quando o
caminho da mesma worktree continua limpo, não deletado, com mesmo tamanho, mesmo `mtime` e hash persistido
válido, o indexador reutiliza a identidade do documento sem reler o arquivo. Essa otimização é deliberadamente
conservadora: arquivos modificados, deletados, com status desconhecido ou futuramente marcados por overlay Git
voltam para o caminho de revalidação/reindexação.

## Reuso de documentos

O storage expõe consultas separadas para reuso por `contentHash` e por `gitBlobHash`. Essas consultas
retornam apenas a identidade reutilizável do documento, não uma entrada de arquivo completa, porque o
mesmo conteúdo pode aparecer em múltiplos caminhos, branches ou worktrees.

O catálogo de arquivos continua sendo o responsável por vincular uma identidade de documento ao caminho
visível na worktree atual. Essa separação evita tratar `path` como identidade de conteúdo e prepara o
indexador incremental para priorizar reuso por blob Git antes de reler arquivos da working tree.

Quando o indexador recebe metadados Git confiáveis para um arquivo limpo, ele consulta primeiro o reuso
por blob hash. Se o blob já está no storage, uma nova entrada de catálogo é criada apontando para o mesmo
documento endereçado por conteúdo, sem abrir nem hashear o arquivo da working tree. Arquivos modificados,
adicionados localmente ou sem blob confiável continuam passando pelo caminho de hash de conteúdo até o
overlay Git completo ser aplicado.

## Overlay da working tree

O estado Git participa da decisão incremental antes de qualquer reuso por tamanho e `mtime`. Entradas
limpas podem reutilizar o catálogo persistido ou um documento conhecido por blob hash. Entradas modificadas
localmente são sempre revalidadas pela leitura da working tree, mesmo quando tamanho e `mtime` ainda
coincidem com o catálogo anterior, porque o Git já informou que o conteúdo visível ao usuário mudou.

Entradas deletadas publicam uma lápide na nova geração quando havia documento anterior para o caminho.
Essa lápide mantém a identidade de conteúdo anterior apenas como referência histórica, mas marca o arquivo
como `deleted` para que buscas indexadas futuras não retornem resultados obsoletos da geração versionada.
Arquivos deletados sem entrada anterior são contabilizados como removidos, mas não criam documento novo.

## Hash de conteúdo

O algoritmo inicial de hash de conteúdo é SHA-256. A escolha privilegia correção, estabilidade e baixa
probabilidade prática de colisão para deduplicação local, mesmo quando o mesmo conteúdo aparece em
branches, worktrees ou caminhos diferentes.

O cálculo é feito em streaming para arquivos, com cancelamento cooperativo entre blocos, evitando carregar
arquivos grandes inteiros em memória. O benchmark `uburu-content-hash-benchmark` mede throughput em um
dataset sintético determinístico e deve ser usado para comparar compiladores, flags e plataformas antes de
trocar o algoritmo ou adicionar uma implementação acelerada.

## Indexação inicial

`PersistentIndexService` implementa a primeira indexação persistente. Ele recebe uma lista de `FileEntry`,
calcula SHA-256 em streaming para arquivos textuais, reutiliza documentos já conhecidos por hash de
conteúdo e publica uma nova geração no storage.

O progresso é reportado por callback, com total, processados, indexados, reutilizados e falhas. O
cancelamento é cooperativo: se o token é sinalizado antes ou durante o cálculo de hash, a operação retorna
`cancelled` e não publica uma geração parcial. Falhas isoladas de leitura são contabilizadas e não impedem
a publicação dos documentos válidos restantes.
