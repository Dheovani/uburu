# Consciência de Git

`RepositoryInfo` representa o repositório lógico e seu diretório Git comum. `WorktreeInfo` representa uma árvore física, seu HEAD e sua branch opcional. Branch ausente com HEAD válido representa detached HEAD.

O backend libgit2 deverá descobrir repositórios, resolver worktrees, ler status e obter OIDs de blobs. Git CLI será apenas fallback isolado. Mudanças em `HEAD`, no index e nas refs relevantes disparam reconciliação incremental.

A visão pesquisável é sempre:

```text
conteúdo do commit/índice + overlay da árvore de trabalho
```

Assim, arquivos modificados e não rastreados substituem a versão indexada, enquanto arquivos deletados são ocultados. Submodules são fronteiras explícitas: podem ser ignorados ou indexados como repositórios próprios, nunca percorridos acidentalmente.
