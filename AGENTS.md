# AGENTS.md

## Visão do projeto

Este projeto é uma aplicação desktop de busca avançada em arquivos, inspirada em ferramentas como Agent Ransack, ripgrep e IDE search, mas com foco especial em repositórios de software, versionamento Git, indexação incremental e alta performance.

O objetivo não é criar apenas um MVP. O objetivo é construir, ao longo do tempo, o melhor sistema possível para busca local em bases de código e diretórios complexos.

A aplicação deve permitir que o usuário escolha um diretório/repositório, pesquise por texto, regex, nomes de arquivo, extensões, símbolos e conteúdo, recebendo resultados rapidamente, com visualização contextual, destaque das ocorrências e integração inteligente com o estado atual do Git.

## Prioridades fundamentais

A prioridade absoluta é eficiência.

A ordem de importância do projeto é:

1. Correção dos resultados.
2. Velocidade perceptível para o usuário.
3. Baixo consumo de memória.
4. Arquitetura limpa e extensível.
5. Experiência de uso refinada.
6. Portabilidade.
7. Facilidade de manutenção.

Não sacrifique a arquitetura para entregar rapidamente. Prefira uma base sólida, modular e testável.

## Stack principal

Use preferencialmente:

* Linguagem: C++23.
* Interface: Qt 6 com QML/Qt Quick.
* Build system: CMake.
* Testes: Catch2 ou GoogleTest.
* Banco local: SQLite.
* Regex: PCRE2 com JIT, quando disponível.
* Git integration: libgit2, quando fizer sentido; Git CLI pode ser usado apenas como fallback ou em pontos isolados.
* Monitoramento de arquivos:

  * Windows: ReadDirectoryChangesW.
  * Linux: inotify.
  * macOS: FSEvents.
  * Qt QFileSystemWatcher pode ser usado inicialmente, mas o design deve permitir backends nativos mais eficientes.
* Empacotamento:

  * Windows: MSIX/installer tradicional.
  * Linux: AppImage/Flatpak.
  * macOS: bundle `.app`.

A interface deve ser separada do core. O motor de busca não deve depender de QML.

## Diretriz arquitetural

A aplicação deve ser dividida em camadas claras:

```txt
UI/QML
  ↓
Application Controller
  ↓
Search Service
  ↓
Search Engine / Index Engine / Git Service / File System Service
  ↓
Platform Adapters
```

O core de busca deve ser utilizável sem interface gráfica. Ele deve poder ser testado por linha de comando ou por testes automatizados.

Estrutura sugerida:

```txt
/
  AGENTS.md
  CMakeLists.txt
  README.md
  docs/
    architecture.md
    indexing.md
    git-awareness.md
    performance.md
    search-semantics.md
  apps/
    desktop/
      CMakeLists.txt
      src/
        main.cpp
        qml/
        ui/
  src/
    core/
      search/
      index/
      filesystem/
      git/
      text/
      platform/
      storage/
      diagnostics/
    app/
      controllers/
      services/
    shared/
      types/
      utils/
  tests/
    unit/
    integration/
    fixtures/
  benchmarks/
```

## Módulos principais

### `core/search`

Responsável por busca direta sem índice, matching textual, regex, busca por nome de arquivo e combinação de filtros.

Deve suportar:

* Busca literal.
* Busca case-sensitive e case-insensitive.
* Busca regex.
* Busca por palavra inteira.
* Busca por extensão.
* Busca por glob.
* Busca por diretórios incluídos/excluídos.
* Busca em arquivos ocultos, quando habilitada.
* Busca respeitando `.gitignore`, quando habilitada.
* Busca com cancelamento.
* Busca com resultados progressivos.
* Busca com limite configurável de resultados.
* Busca com limite de tamanho de arquivo.
* Detecção de arquivos binários.

### `core/index`

Responsável por indexação persistente.

A indexação deve ser incremental, Git-aware e content-addressed sempre que possível.

Não trate o índice apenas como uma lista de caminhos. O mesmo caminho pode ter conteúdos diferentes em branches diferentes.

O índice deve considerar:

* `repository_id`
* `worktree_id`
* caminho absoluto da worktree
* caminho relativo do arquivo
* tamanho
* mtime
* hash de conteúdo
* hash de blob Git, quando disponível
* branch atual
* HEAD atual
* status do arquivo
* arquivos deletados
* arquivos ignorados
* arquivos não rastreados
* arquivos modificados localmente

Modelo conceitual:

```txt
Repository
  └── Worktree
        ├── Current HEAD
        ├── Current branch
        ├── Indexed file catalog
        ├── Content-addressed documents
        └── Working tree overlay
```

A busca deve representar o estado real da árvore de trabalho visível ao usuário, não apenas o último commit.

### `core/git`

Responsável por detectar e interpretar o estado Git.

Deve suportar:

* Detectar se uma pasta é repositório Git.
* Detectar diretório `.git`.
* Detectar worktrees.
* Detectar branch atual.
* Detectar HEAD atual.
* Detectar mudanças de branch.
* Detectar mudanças em `.git/HEAD`.
* Detectar mudanças em `.git/index`.
* Identificar arquivos rastreados.
* Identificar arquivos não rastreados.
* Identificar arquivos ignorados.
* Identificar arquivos modificados.
* Obter blob hash de arquivos rastreados, quando possível.
* Lidar corretamente com detached HEAD.
* Lidar corretamente com submodules.
* Lidar corretamente com múltiplas worktrees.

O sistema deve tratar troca de branch como alteração estrutural importante, mas não deve necessariamente reconstruir todo o índice. Sempre que possível, reutilize documentos já indexados por hash de conteúdo ou blob hash.

### `core/filesystem`

Responsável por varredura e monitoramento do sistema de arquivos.

Deve suportar:

* Scan recursivo eficiente.
* Filtros por diretório.
* Filtros por extensão.
* Detecção de binários.
* Detecção de encoding.
* Leitura em blocos.
* Processamento paralelo.
* Priorização de arquivos menores ou mais prováveis.
* Backpressure para não sobrecarregar a UI.
* Cancelamento cooperativo.
* Watchers nativos por plataforma.
* Normalização de caminhos.
* Tratamento de symlinks.
* Proteção contra ciclos de diretórios.
* Configuração para seguir ou ignorar symlinks.

### `core/text`

Responsável por leitura, normalização e matching de texto.

Deve considerar:

* UTF-8.
* UTF-16 LE/BE.
* Latin-1 quando necessário.
* Arquivos com BOM.
* Quebras de linha `LF` e `CRLF`.
* Normalização Unicode opcional.
* Busca case-insensitive robusta.
* Extração de linhas.
* Extração de contexto antes/depois.
* Highlight das ocorrências.
* Busca em arquivos grandes sem carregar tudo em memória, quando possível.

### `core/storage`

Responsável por persistência local.

Use SQLite para:

* Catálogo de repositórios.
* Catálogo de worktrees.
* Metadados dos arquivos.
* Estado do índice.
* Histórico de buscas.
* Preferências do usuário.
* Configurações por repositório.
* Cache de resultados, quando fizer sentido.
* Diagnósticos de performance.
* Estatísticas de indexação.

Evite depender exclusivamente de SQLite FTS5 como solução definitiva para o motor de busca. Ele pode ser usado como componente, mas a arquitetura deve permitir troca ou evolução do backend de indexação.

### `core/diagnostics`

Responsável por logging, métricas e profiling interno.

Deve medir:

* Tempo de scan.
* Tempo de indexação.
* Tempo até o primeiro resultado.
* Tempo total da busca.
* Arquivos processados por segundo.
* Bytes processados por segundo.
* Tempo de matching.
* Tempo de renderização ou envio de resultados para UI.
* Uso aproximado de memória.
* Número de arquivos ignorados.
* Número de arquivos binários ignorados.
* Número de arquivos reindexados.
* Número de documentos reutilizados por hash.

A aplicação deve ter uma tela ou modo de diagnóstico para ajudar no desenvolvimento.

## Modelo de busca

A aplicação deve suportar dois modos principais:

### Busca direta

Busca diretamente nos arquivos da árvore atual.

Use este modo quando:

* O índice ainda não existe.
* O usuário quer resultado imediatamente.
* O diretório é pequeno.
* A busca é específica.
* O índice está desatualizado.

### Busca indexada

Consulta índice persistente.

Use este modo quando:

* O repositório já foi indexado.
* O usuário faz buscas repetidas.
* A base de arquivos é grande.
* O usuário deseja busca rápida por conteúdo, nome, extensão, símbolos ou metadados.

A interface pode combinar os dois modos:

```txt
1. Mostrar resultados rápidos vindos do índice.
2. Continuar validando/refinando com leitura direta do estado atual.
3. Atualizar ou remover resultados obsoletos.
```

## Git e versionamento

O índice deve ser consciente de versionamento.

Conceitos importantes:

```txt
Repository:
  Repositório Git lógico.

Worktree:
  Diretório físico atualmente aberto.

Commit:
  Estado versionado.

Branch:
  Ponteiro para commit.

Blob:
  Conteúdo versionado de um arquivo.

Working tree overlay:
  Alterações locais não commitadas, arquivos novos, arquivos modificados e arquivos deletados.
```

Nunca assuma que `path` identifica unicamente o conteúdo de um documento.

Use uma chave conceitual mais rica:

```txt
repository_id
worktree_id
relative_path
content_hash
git_blob_hash
```

Ao trocar de branch:

* Detecte alteração de HEAD.
* Detecte alteração em `.git/index`.
* Faça rescan incremental.
* Remova ou oculte arquivos que não existem mais.
* Reutilize conteúdo já indexado por hash.
* Reindexe apenas arquivos novos ou modificados.
* Preserve histórico de buscas e preferências.

A busca deve refletir o estado atual da worktree, incluindo alterações locais não commitadas.

## Interface de usuário

A interface deve ser rápida, limpa e produtiva.

A tela principal deve conter:

* Campo de busca principal.
* Seletor de diretório/repositório.
* Opções de busca:

  * texto literal
  * regex
  * case-sensitive
  * palavra inteira
  * incluir ignorados
  * incluir ocultos
  * incluir binários
  * respeitar `.gitignore`
* Filtro por extensão.
* Filtro por diretório.
* Filtro por tamanho.
* Lista de resultados.
* Preview do arquivo.
* Highlight das ocorrências.
* Contagem de resultados.
* Tempo até o primeiro resultado.
* Status da indexação.
* Botão cancelar.
* Histórico de buscas.
* Favoritos ou buscas salvas.

A UI deve receber resultados progressivamente. Não espere a busca terminar para mostrar resultados.

A aplicação nunca deve travar a interface durante busca, indexação ou leitura de arquivos.

## Performance

Regras obrigatórias:

* Nunca processe busca pesada na thread da UI.
* Use cancelamento cooperativo.
* Use filas thread-safe para resultados.
* Evite carregar arquivos enormes inteiros em memória.
* Evite regex quando busca literal for suficiente.
* Use PCRE2 JIT para regex quando disponível.
* Faça batch de resultados enviados à UI.
* Use debounce no campo de busca.
* Use cache com invalidação explícita.
* Evite reindexação total desnecessária.
* Prefira indexação incremental.
* Meça performance antes de otimizar agressivamente.

O tempo até o primeiro resultado é uma métrica de primeira classe.

## Concorrência

O sistema deve ter design seguro para concorrência.

Use:

* Threads de worker.
* Thread pool.
* Fila de tarefas.
* Tokens de cancelamento.
* Result streaming.
* Limites de memória.
* Limites de profundidade.
* Prioridade de tarefas.

Evite:

* Data races.
* Bloqueios longos.
* Atualização direta da UI a partir de threads de worker.
* Uso excessivo de mutexes globais.
* Estado compartilhado desnecessário.

## Configurações

Configurações globais:

* Tema claro/escuro/sistema.
* Idioma.
* Tamanho máximo de arquivo.
* Diretórios ignorados por padrão.
* Extensões ignoradas.
* Incluir arquivos ocultos.
* Respeitar `.gitignore`.
* Número de threads.
* Local do banco de índice.
* Limite de resultados por busca.

Configurações por repositório:

* Nome amigável.
* Caminho.
* Preferências de ignore.
* Extensões relevantes.
* Buscas salvas.
* Estado de indexação.
* Última branch aberta.
* Último HEAD indexado.

## Internacionalização

Desde o início, a interface deve estar preparada para i18n.

Idioma inicial obrigatório:

* `pt-BR`

Também preparar estrutura para:

* `en-US`

Não escreva textos fixos diretamente em QML ou C++ quando forem visíveis ao usuário. Centralize mensagens traduzíveis.

O português deve usar acentuação correta e norma culta.

## Qualidade de código

Use C++ moderno.

Preferir:

* RAII.
* Tipos fortes.
* `std::filesystem`.
* `std::optional`.
* `std::variant`, quando fizer sentido.
* `std::string_view`, com cuidado.
* `std::chrono`.
* `std::jthread`, se disponível.
* Separação clara entre interfaces e implementações.
* Erros explícitos.
* Testes automatizados.

Evitar:

* Estado global mutável.
* Ponteiros crus sem necessidade.
* Código acoplado à UI.
* Funções gigantes.
* Classes “Deus”.
* Otimizações obscuras sem benchmark.
* Silenciar erros importantes.
* Misturar lógica de busca com renderização.

## Testes

Criar testes para:

* Busca literal.
* Busca case-insensitive.
* Busca regex.
* Busca com palavra inteira.
* Busca em arquivos UTF-8.
* Busca em arquivos com CRLF.
* Busca ignorando binários.
* Busca respeitando `.gitignore`.
* Scanner recursivo.
* Symlinks.
* Cancelamento.
* Indexação incremental.
* Troca de branch.
* Arquivos deletados.
* Arquivos modificados.
* Arquivos não rastreados.
* Worktrees.
* Reutilização por hash.
* Ordenação de resultados.
* Highlight de ocorrências.
* Extração de contexto.

Use fixtures pequenas e determinísticas.

## Benchmarks

Criar benchmarks para:

* Scan de muitos arquivos pequenos.
* Scan de poucos arquivos grandes.
* Busca literal.
* Busca regex.
* Tempo até primeiro resultado.
* Indexação inicial.
* Reindexação incremental.
* Troca de branch.
* Reuso de documentos por hash.
* Carga de resultados na UI.

Benchmarks devem ser fáceis de executar e comparar.

## Documentação

Manter documentação em `docs/`.

Arquivos esperados:

```txt
docs/architecture.md
docs/indexing.md
docs/git-awareness.md
docs/search-semantics.md
docs/performance.md
docs/storage.md
docs/ui.md
docs/build.md
```

Sempre que uma decisão arquitetural importante for tomada, registre em documentação.

## Estilo de desenvolvimento

### Formatação de C++

Todo código C++ novo ou alterado em `apps/`, `src/` e `tests/` deve respeitar integralmente o
`.clang-format` da raiz, configurado para C++23. Antes de concluir uma alteração:

* Use indentação de 2 espaços, nunca tabs.
* Mantenha o limite de 100 colunas.
* Use `Type* name` e `Type& name` para ponteiros e referências.
* Preserve a ordenação case-sensitive de includes.
* Formate com `cmake --build <diretório-de-build> --target format` ou execute `clang-format -i`
  apenas nos arquivos C++ próprios alterados.
* Não formate dependências vendorizadas nem arquivos gerados automaticamente.

O target `format` é auxiliar e nunca deve ser requisito para compilar o projeto.

### Nomes de arquivos

Todo arquivo próprio novo ou renomeado deve usar `kebab-case`, somente com letras minúsculas,
números e hífens entre palavras. Exemplos válidos: `search-engine.hpp`,
`direct-search-engine-tests.cpp` e `git-awareness.md`.

As únicas exceções são nomes canônicos exigidos ou amplamente convencionados pelas ferramentas:
`CMakeLists.txt`, `README.md`, `AGENTS.md`, `TODO.md`, `.clang-format`, `.gitignore` e manifestos
como `vcpkg.json`. Arquivos gerados automaticamente e dependências vendorizadas também preservam
seus nomes originais.

Ao renomear um arquivo, atualize no mesmo trabalho todos os includes, targets CMake, recursos,
documentação e testes que o referenciem. Não introduza novos nomes em `snake_case`, `PascalCase` ou
`camelCase` no sistema de arquivos.

Ao implementar algo:

1. Entenda a arquitetura existente.
2. Não quebre separação entre core e UI.
3. Prefira interfaces pequenas.
4. Escreva testes para comportamento central.
5. Documente decisões relevantes.
6. Meça quando a mudança envolver performance.
7. Preserve compatibilidade multiplataforma.
8. Evite atalhos que dificultem indexação avançada no futuro.

## Definição de qualidade

Uma funcionalidade só deve ser considerada boa quando:

* Funciona corretamente.
* Tem tratamento de erro.
* Não bloqueia a UI.
* É testável.
* É extensível.
* Não piora a arquitetura.
* Não assume apenas Windows.
* Não ignora Git/worktree quando isso for relevante.
* Não depende de comportamento acidental.

## Não fazer

Não criar uma aplicação Electron.

Não criar o core de busca em JavaScript.

Não acoplar a busca ao QML.

Não depender exclusivamente de comandos externos como `grep`, `find` ou `rg`.

Não indexar apenas por caminho.

Não ignorar branch, HEAD, worktree e estado local do Git.

Não reconstruir o índice inteiro sem necessidade.

Não bloquear a UI durante operações longas.

Não adicionar textos visíveis ao usuário sem passar pelo sistema de i18n.

Não aceitar código sem testes para partes críticas do core.

Não tratar performance como detalhe secundário.

## Filosofia do produto

Este projeto deve ser tratado como uma ferramenta séria para desenvolvedores e usuários avançados.

O sistema deve ser rápido o suficiente para uso cotidiano, confiável o suficiente para substituir buscas manuais e inteligente o suficiente para entender repositórios versionados.

A meta de longo prazo é uma aplicação local de busca extremamente eficiente, com excelente experiência de usuário, capaz de lidar com bases de código grandes, múltiplas branches, worktrees, arquivos modificados e indexação incremental sem perder precisão.
