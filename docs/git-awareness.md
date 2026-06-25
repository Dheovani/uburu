# Consciência de Git

`RepositoryInfo` representa o repositório lógico, seu diretório Git comum e, quando disponível, a raiz 
da worktree descoberta. `WorktreeInfo` representa uma árvore física, seu diretório Git específico, seu
HEAD e sua branch opcional. Branch ausente com HEAD válido representa detached HEAD.

O backend `Libgit2GitService` é a implementação inicial de `GitService`. Ele descobre repositórios via
libgit2, resolve diretório Git comum, raiz da worktree, branch atual, detached HEAD, OID de `HEAD`,
worktrees linked, status de arquivos e OID de blobs rastreados. O status já diferencia arquivos limpos,
adicionados ao índice, não rastreados, ignorados, modificados, deletados e conflitantes. As operações
retornam `GitResult<T>` com `GitErrorCode` tipado, para diferenciar ausência de repositório, falha de
leitura e backend indisponível. Git CLI será apenas fallback isolado em adapter explícito. Mudanças em
`HEAD`, no index e nas refs relevantes disparam reconciliação incremental.

`GitService::change_state()` expõe um snapshot comparável do estado Git visível para uma worktree:
branch atual, `HEAD`, detached HEAD e assinaturas de `HEAD`, `index`, ref da branch e `packed-refs`.
Esse snapshot não substitui watchers de filesystem; ele define o estado que os watchers devem comparar
para decidir quando invalidar deltas e iniciar reconciliação incremental.

A visão pesquisável é sempre:

```text
conteúdo do commit/índice + overlay da árvore de trabalho
```

Assim, arquivos modificados e não rastreados substituem a versão indexada, enquanto arquivos deletados são ocultados. Submodules são fronteiras explícitas: podem ser ignorados ou indexados como repositórios próprios, nunca percorridos acidentalmente.
