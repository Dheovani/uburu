# Consciência de Git

`RepositoryInfo` representa o repositório lógico, seu diretório Git comum e, quando disponível, a raiz 
da worktree descoberta. `WorktreeInfo` representa uma árvore física, seu diretório Git específico, seu
HEAD e sua branch opcional. Branch ausente com HEAD válido representa detached HEAD.

O backend `Libgit2GitService` é a implementação inicial de `GitService`. Ele descobre repositórios via
libgit2, resolve diretório Git comum, raiz da worktree, branch atual, detached HEAD, OID de `HEAD`,
worktrees linked, status de arquivos e OID de blobs rastreados. O status já diferencia arquivos limpos,
adicionados ao índice, não rastreados, ignorados, modificados, deletados e conflitantes. As operações
retornam `GitResult<T>` com `GitErrorCode` tipado, para diferenciar ausência de repositório, falha de
leitura e backend indisponível. O fallback por Git CLI existe somente como `GitCliGitService`, um
adapter explícito que não é usado implicitamente pelo core. Mudanças em `HEAD`, no index e nas refs
relevantes disparam reconciliação incremental.

`GitService::changeState()` expõe um snapshot comparável do estado Git visível para uma worktree:
branch atual, `HEAD`, detached HEAD e assinaturas de `HEAD`, `index`, ref da branch e `packed-refs`.
Esse snapshot não substitui watchers de filesystem; ele define o estado que os watchers devem comparar
para decidir quando invalidar deltas e iniciar reconciliação incremental.

`planReconciliation()` compara dois snapshots de `GitChangeState` e transforma diferenças em um plano
explícito. Mudança de branch, mudança de `HEAD`, entrada ou saída de detached HEAD e alteração de refs
relevantes exigem reconciliação estrutural, mas ainda permitem reutilizar documentos por blob hash. Uma
mudança apenas no index exige reconciliação do overlay local, sem tratar a geração versionada como
estruturalmente obsoleta.

A visão pesquisável é sempre:

```text
conteúdo do commit/índice + overlay da árvore de trabalho
```

Assim, arquivos modificados e não rastreados substituem a versão indexada, enquanto arquivos deletados
são ocultados. Submodules são fronteiras explícitas: podem ser ignorados ou indexados como repositórios
próprios, nunca percorridos acidentalmente.

## Overlay da working tree

`GitService::workingTreeOverlay()` materializa a diferença entre a geração versionada e a árvore de
trabalho visível ao usuário. Cada `GitOverlayEntry` carrega:

- `relativePath`, o caminho atual que deve aparecer na busca;
- `previousRelativePath`, quando libgit2 identifica rename ou move;
- `status`, o estado Git bruto normalizado para os tipos do Uburu;
- `disposition`, a decisão de busca/indexação sobre a geração versionada;
- `reusableBlob`, quando o conteúdo versionado pode ser reaproveitado por hash de blob.

As disposições iniciais são:

- `useIndexedContent`: o documento versionado continua válido;
- `replaceWithWorkingTree`: a worktree substitui o conteúdo versionado;
- `addWorkingTreeFile`: arquivo novo deve entrar como overlay local;
- `hideIndexedContent`: arquivo deletado localmente não deve aparecer como resultado obsoleto;
- `conflict`: arquivo em conflito deve ser tratado de forma conservadora.

Renames e moves não são tratados como exclusão seguida de adição quando libgit2 fornece o caminho
anterior. O overlay preserva `previousRelativePath` e tenta resolver `reusableBlob` no caminho antigo,
permitindo que o futuro índice reutilize documentos por blob hash em vez de reler conteúdo já conhecido.

## Fronteiras de repositório

`GitService::repositoryBoundary()` diferencia três casos:

- `none`: o caminho pertence normalmente à worktree atual;
- `submodule`: o caminho é registrado como submodule do repositório atual;
- `nestedRepository`: o caminho contém um `.git` próprio, mas não é submodule registrado.

Submodules e repositórios aninhados são fronteiras explícitas. O scanner e o índice podem oferecer uma
política para ignorar, atravessar conscientemente ou indexar como outro escopo, mas não devem entrar
nesses repositórios por acidente como se fossem diretórios comuns.

## Worktrees indisponíveis

`WorktreeInfo` representa também worktrees linked bloqueadas ou prunable. Uma worktree bloqueada expõe
`locked` e `lockReason`; uma worktree removida fisicamente, mas ainda registrada pelo Git, expõe
`prunable`. O índice deve tratar esses estados como metadados estruturais: não apagar histórico nem cache
imediatamente, mas também não presumir que a árvore física pode ser escaneada.

## Algoritmo de hash Git

Hashes de objetos Git são representados por `GitObjectId`, que carrega o algoritmo (`sha1`, `sha256` ou
`unknown`) junto com o valor textual. O libgit2 atualmente valida o caminho SHA-1 nos testes e o contrato
já está preparado para repositórios SHA-256 quando o ambiente e as dependências expuserem esse modo.
