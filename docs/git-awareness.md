# Consciência de Git

`RepositoryInfo` representa o repositório lógico, seu diretório Git comum e, quando disponível, a raiz 
da worktree descoberta. `WorktreeInfo` representa uma árvore física, seu diretório Git específico, seu
HEAD e sua branch opcional. Branch ausente com HEAD válido representa detached HEAD.

O backend `Libgit2GitService` é a implementação inicial de `GitService`. Ele descobre repositórios via
libgit2, resolve diretório Git comum, raiz da worktree, branch atual, detached HEAD, OID de `HEAD`,
status de arquivos e OID de blobs rastreados. As operações retornam `GitResult<T>` com `GitErrorCode`
tipado, para diferenciar ausência de repositório, falha de leitura e backend indisponível. Git CLI será
apenas fallback isolado em adapter explícito. Mudanças em `HEAD`, no index e nas refs relevantes
disparam reconciliação incremental.

A visão pesquisável é sempre:

```text
conteúdo do commit/índice + overlay da árvore de trabalho
```

Assim, arquivos modificados e não rastreados substituem a versão indexada, enquanto arquivos deletados são ocultados. Submodules são fronteiras explícitas: podem ser ignorados ou indexados como repositórios próprios, nunca percorridos acidentalmente.
