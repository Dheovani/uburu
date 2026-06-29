# TODO — Uburu

Este documento é o plano operacional do projeto. A ordem dos marcos representa dependências reais:
itens posteriores não devem degradar correção, cancelamento, uso de memória ou separação entre core
e UI para avançarem mais rapidamente.

## Convenções do backlog

- `[ ]` pendente; `[x]` concluído e validado.
- `P0` bloqueia uma base confiável; `P1` compõe o produto principal; `P2` profissionaliza e amplia; `P3` é evolução avançada.
- Um item só pode ser marcado como concluído quando código, testes, documentação e métricas aplicáveis estiverem atualizados.
- Alterações críticas do core exigem teste automatizado. Alterações de performance exigem benchmark antes/depois ou uma métrica reproduzível.
- Textos visíveis devem existir em `pt-BR` e `en-US`.

## Estado atual validado

- [x] Biblioteca `uburu_core` independente de Qt.
- [x] Camada de aplicação separada da UI.
- [x] Aplicação Qt Quick mínima com busca assíncrona e cancelamento.
- [x] Scanner recursivo inicial e busca literal linha a linha.
- [x] Tipos centrais para repositório, worktree, documentos e resultados.
- [x] Interfaces iniciais de busca, filesystem, índice, Git e storage.
- [x] Escopo de busca com múltiplas raízes e exclusões por raiz no core.
- [x] Build C++23 com CMake, Qt 6.11.1/MinGW e vcpkg.
- [x] Testes unitários iniciais com Catch2.
- [x] Formatação automática e convenção kebab-case.

## Marco 0 — Engenharia reproduzível e higiene do repositório (P0)

- [x] Inicializar e documentar o repositório Git, branch principal e política de commits.
- [x] Adicionar `CMakePresets.json` para Windows/MinGW, Windows/MSVC, Linux e desenvolvimento apenas do core.
- [x] Eliminar comandos dependentes de caminhos locais fixos como `C:\Qt\6.11.1`.
- [x] Corrigir `README.md` e `docs/build.md` com o fluxo MinGW atualmente validado.
- [x] Adicionar scripts ou presets para configurar, compilar, testar, formatar e executar a aplicação.
- [x] Criar target ou script de implantação local com `windeployqt` e DLLs do vcpkg.
- [x] Adicionar `.editorconfig` coerente com `.clang-format` e arquivos Markdown/QML.
- [x] Adicionar análise estática com clang-tidy e configurar conjunto inicial de regras.
- [x] Adicionar verificação de warnings como erros em CI, com exceções justificadas por compilador.
- [x] Adicionar sanitizers em plataformas compatíveis: AddressSanitizer e UndefinedBehaviorSanitizer.
- [x] Adicionar verificação automática de formatação sem modificar arquivos (`format-check`).
- [x] Criar CI para Windows, Linux e, quando viável, macOS.
- [x] Executar em CI: configure, build, testes, format-check e análise estática.
- [x] Definir política de versões mínimas de CMake, Qt, compiladores e vcpkg.
- [x] Fixar baseline do vcpkg para builds reproduzíveis.
- [x] Adicionar `LICENSE`, `CONTRIBUTING.md`, código de conduta e política de segurança.
- [x] Documentar licenças das dependências e obrigações de redistribuição do Qt.

### Critério de saída

- [x] Um clone limpo pode ser configurado e testado por preset sem editar caminhos manualmente.
- [ ] CI verde em pelo menos Windows/MinGW e Linux.

## Marco 1 — Semântica correta da busca direta (P0)

- [x] Especificar formalmente a semântica em `docs/search-semantics.md`.
- [x] Validar `SearchQuery` e retornar erros tipados para raiz inválida, expressão vazia e opções
  incompatíveis.
- [x] Encontrar todas as ocorrências de uma linha, não apenas a primeira.
- [x] Definir e testar comportamento para ocorrências sobrepostas.
- [x] Implementar busca case-sensitive e case-insensitive Unicode de forma consistente.
- [x] Implementar palavra inteira com regras Unicode e opção específica para identificadores de código.
- [x] Implementar busca regex com PCRE2.
- [x] Habilitar PCRE2 JIT quando suportado e fornecer fallback explícito.
- [x] Limitar tempo, profundidade e recursos de regex para evitar padrões patológicos.
- [x] Retornar erro de compilação de regex com posição e mensagem traduzível.
- [x] Implementar busca por nome de arquivo separada da busca por conteúdo.
- [x] Implementar filtros por glob, extensão, diretório e tamanho.
- [x] Normalizar extensões e definir sensibilidade a maiúsculas por plataforma.
- [x] Implementar include/exclude com precedência documentada.
- [x] Aplicar limite de resultados global e por arquivo.
- [x] Implementar ordenação determinística e estratégia de relevância inicial.
- [x] Preservar resultados progressivos sem esperar a conclusão da varredura.
- [x] Distinguir cancelamento, falha parcial e conclusão normal no resumo.
- [x] Propagar erros de leitura sem interromper silenciosamente toda a busca.
- [x] Definir comportamento para arquivos alterados ou removidos durante a leitura.
- [x] Evitar cópias desnecessárias de linhas, caminhos e resultados.

### Testes obrigatórios

- [x] Literal com múltiplas ocorrências na mesma linha.
- [x] Case-sensitive e case-insensitive com ASCII e Unicode.
- [x] Palavra inteira, identificadores, pontuação e limites Unicode.
- [x] Regex válida, inválida, JIT/fallback e cancelamento.
- [x] CRLF, LF, arquivo sem newline final e linhas vazias.
- [x] Limite de resultados, filtros e ordenação determinística.
- [x] Erros de permissão, arquivo removido e leitura parcial.

## Marco 2 — Texto, encoding e arquivos grandes (P0)

- [x] Criar abstração de leitor de texto streaming no módulo `core/text`.
- [x] Detectar BOM e suportar UTF-8, UTF-16 LE e UTF-16 BE.
- [x] Definir fallback configurável para Latin-1 e encoding desconhecido.
- [x] Validar UTF-8 e definir política explícita para sequências inválidas.
- [x] Detectar binários usando amostragem robusta, não apenas byte NUL por linha.
- [x] Tornar tamanho da amostra e política de binários configuráveis.
- [x] Ler arquivos grandes em blocos sem perder matches nas fronteiras.
- [x] Preservar offsets corretos entre bytes, code points, linha e coluna visual.
- [x] Extrair contexto anterior/posterior sem carregar o arquivo inteiro.
- [x] Produzir spans de highlight para múltiplas ocorrências.
- [x] Suportar LF, CRLF e CR isolado de maneira documentada.
- [x] Definir limite para linha extremamente longa.

### Critério de saída

- [x] Buscar em arquivos maiores que o orçamento de memória sem alocação proporcional ao tamanho.
- [x] Fixtures determinísticas cobrem todos os encodings e finais de linha suportados.

## Marco 3 — Filesystem, ignore e concorrência (P0)

- [x] Implementar `.gitignore` real com regras aninhadas, negação e precedência.
- [x] Suportar arquivos globais de ignore do Git e `.git/info/exclude` quando configurado.
- [x] Separar arquivos ocultos, ignorados e binários nas métricas.
- [x] Aplicar corretamente diretórios incluídos e excluídos no scanner.
- [x] Normalizar caminhos absolutos e relativos por plataforma.
- [x] Tratar caminhos longos, UNC e diferenças de caixa no Windows.
- [x] Detectar arquivos esparsos e definir política de leitura por plataforma.
- [x] Definir política para junctions, symlinks e mount points.
- [x] Detectar ciclos ao seguir symlinks.
- [x] Implementar pool de workers com tamanho configurável.
- [x] Priorizar arquivos pequenos e candidatos mais prováveis sem quebrar determinismo final.
- [x] Adicionar fila limitada e backpressure entre scan, leitura, matching e publicação.
- [x] Garantir cancelamento cooperativo em scan, fila, leitura e matching.
- [x] Evitar mutex global e medir contenção.
- [x] Implementar watchers por interface comum.
- [x] Implementar backend Windows com `ReadDirectoryChangesW`.
- [x] Implementar backend Linux com `inotify`.
- [x] Implementar backend macOS com `FSEvents`.
- [x] Tratar overflow/perda de eventos com rescan de reconciliação.
- [x] Adicionar fallback inicial documentado quando backend nativo não estiver disponível.

## Marco 4 — Integração Git completa (P1)

- [x] Implementar `GitService` com libgit2 e erros tipados.
- [x] Descobrir repositório comum, `.git` arquivo/diretório e raiz da worktree.
- [x] Gerar identificadores estáveis para repositório e worktree.
- [x] Detectar branch atual, HEAD e detached HEAD.
- [x] Enumerar múltiplas worktrees.
- [x] Ler arquivos rastreados, não rastreados, ignorados, modificados, deletados e conflitantes.
- [x] Obter blob OID de arquivos rastreados.
- [x] Detectar alterações em HEAD, index e refs relevantes.
- [x] Tratar troca de branch como reconciliação estrutural incremental.
- [x] Modelar overlay local sobre conteúdo versionado.
- [x] Tratar renames e movimentos com reutilização de conteúdo.
- [x] Definir comportamento para submodules e repositórios aninhados.
- [x] Suportar worktrees bloqueadas, removidas e prunable.
- [x] Isolar fallback por Git CLI atrás de adapter explícito.
- [x] Testar repositórios SHA-1 e preparar tipos para SHA-256.

### Testes de integração Git

- [x] Branch normal e detached HEAD.
- [x] Troca de branch com arquivos adicionados, removidos e iguais por blob.
- [x] Modificado local, novo, deletado, ignorado e conflito.
- [x] Múltiplas worktrees e submodule.
- [x] Reuso de conteúdo por blob/hash entre branches e worktrees.

## Marco 5 — Storage SQLite profissional (P1)

- [x] Implementar `StorageService` com RAII e statements preparados.
- [x] Criar sistema versionado de migrations.
- [x] Definir schema para repositories, worktrees, generations, files, documents e overlays.
- [x] Separar identidade de caminho da identidade de conteúdo.
- [x] Armazenar content hash e blob hash com algoritmo/versionamento explícitos.
- [x] Ativar WAL, foreign keys, busy timeout e pragmas medidos.
- [x] Publicar gerações do índice em transação atômica.
- [x] Recuperar de interrupção durante migration ou indexação.
- [x] Validar integridade e oferecer reconstrução segura do índice corrompido.
- [x] Implementar retenção e coleta de documentos órfãos.
- [x] Persistir preferências globais e por repositório.
- [x] Persistir histórico de buscas e buscas salvas.
- [x] Persistir métricas de indexação sem crescimento ilimitado.
- [x] Definir localização padrão do banco por plataforma.
- [x] Permitir localização personalizada e migração do banco.
- [x] Testar concorrência entre leitura de busca e escrita de nova geração.
- [x] Avaliar FTS5 por benchmark, sem acoplar o contrato ao backend.

## Marco 6 — Índice persistente e incremental (P1)

- [ ] Definir formato interno de documento indexado e versioná-lo.
- [ ] Escolher hash de conteúdo com benchmark de throughput e colisão aceitável.
- [ ] Implementar indexação inicial cancelável e progressiva.
- [ ] Implementar catálogo incremental por tamanho, mtime, hash e estado Git.
- [ ] Deduplicar documentos por hash de conteúdo.
- [ ] Reutilizar documentos por blob hash antes de reler arquivos.
- [ ] Aplicar overlay da working tree sobre a geração versionada.
- [ ] Ocultar deletados e substituir modificados sem resultados obsoletos.
- [ ] Reconciliar eventos do watcher em batches transacionais.
- [ ] Detectar staleness do índice e expor estado para a UI.
- [ ] Implementar busca indexada por conteúdo e metadados.
- [ ] Combinar resultado rápido do índice com validação direta.
- [ ] Atualizar, confirmar ou remover resultados durante refinamento.
- [ ] Definir ranking e merge determinísticos entre fontes.
- [ ] Implementar orçamento de disco e política de eviction.
- [ ] Versionar schema e formato para upgrades sem perda desnecessária do cache.
- [ ] Implementar pausa, retomada e reindexação manual.

## Marco 7 — Serviço de busca e observabilidade (P1)

- [ ] Fazer `SearchService` selecionar busca direta, indexada ou híbrida por política explícita.
- [ ] Separar DTOs de aplicação dos tipos de persistência e detalhes dos engines.
- [ ] Criar canal de eventos para progresso, resultados, correções e erros.
- [ ] Adicionar IDs de busca para descartar eventos atrasados de consultas canceladas.
- [ ] Implementar batching adaptativo de resultados para a UI.
- [ ] Medir tempo até o primeiro resultado e tempo total em todas as estratégias.
- [ ] Implementar `MetricsSink` concreto e logging estruturado.
- [ ] Adicionar níveis, categorias e rotação de logs.
- [ ] Remover ou mascarar conteúdo e caminhos sensíveis dos logs por padrão.
- [ ] Medir arquivos/bytes por segundo, filas, cache hit e reuso por hash.
- [ ] Medir memória aproximada e detectar crescimento entre buscas.
- [ ] Criar modo/tela de diagnóstico exportável.
- [ ] Adicionar tracing de uma busca sem penalidade relevante quando desabilitado.

## Marco 8 — Experiência desktop produtiva (P1)

- [ ] Redesenhar a tela principal com layout responsivo e estados vazios claros.
- [ ] Implementar seletor de diretório/repositório com recentes e favoritos.
- [ ] Permitir selecionar múltiplos diretórios/repositórios no seletor visual, com inclusão e
  exclusão de subdiretórios por raiz.
- [ ] Expor todos os filtros previstos sem hardcode de textos.
- [ ] Adicionar debounce configurável e busca ao digitar.
- [ ] Mostrar contagem, arquivos processados, tempo até primeiro resultado e duração total.
- [ ] Mostrar status e progresso da indexação.
- [ ] Virtualizar a lista para centenas de milhares de resultados.
- [ ] Preservar seleção durante batches e refinamento híbrido.
- [ ] Criar agrupamento por arquivo e navegação entre ocorrências.
- [ ] Implementar preview de arquivo assíncrono, cancelável e limitado.
- [ ] Implementar highlight de múltiplas ocorrências e linhas de contexto.
- [ ] Adicionar números de linha, monospace e tab width configurável.
- [ ] Abrir arquivo no editor configurado e copiar caminho/ocorrência.
- [ ] Exibir menu de contexto nativo do sistema operacional ao clicar com botão direito em um
  arquivo listado, equivalente ao menu do Explorer/Finder/gerenciador de arquivos.
- [ ] Oferecer fallback próprio para ações essenciais quando o menu nativo não estiver disponível:
  abrir arquivo, abrir pasta, copiar caminho e copiar ocorrência.
- [ ] Adicionar atalhos de teclado completos e command palette.
- [ ] Implementar histórico, buscas salvas e favoritos.
- [ ] Implementar tema claro, escuro e sistema.
- [ ] Persistir geometria, splitters, filtros e último repositório.
- [ ] Exibir erros parciais sem interromper resultados válidos.
- [ ] Impedir busca regex enquanto backend não estiver disponível ou remover o stub visual enganoso.
- [ ] Tornar cancelamento imediato e visualmente confiável.
- [ ] Testar acessibilidade: foco, teclado, contraste, nomes acessíveis e leitores de tela.
- [ ] Testar DPI alto, múltiplos monitores e escalas fracionárias.
- [ ] Completar e revisar traduções `pt-BR` e `en-US`.
- [ ] Definir estratégia para pluralização, atalhos e strings técnicas.

## Marco 9 — Testes e qualidade contínua (P0/P1)

- [ ] Criar helpers RAII para diretórios e arquivos temporários nos testes.
- [ ] Remover nomes temporários fixos que possam colidir em execução paralela.
- [ ] Criar fixtures pequenas de texto, encoding, ignore e Git.
- [ ] Adicionar testes unitários para cada regra pura de matching e filtro.
- [ ] Adicionar testes de integração de scanner em filesystem real temporário.
- [x] Adicionar testes de integração SQLite com banco descartável.
- [ ] Adicionar testes de integração libgit2 com repositórios descartáveis.
- [ ] Testar cancelamento em diferentes pontos do pipeline.
- [ ] Testar backpressure e limites de memória.
- [ ] Testar concorrência repetidamente e sob ThreadSanitizer onde disponível.
- [ ] Adicionar testes Qt do controller/model e estados observáveis da UI.
- [ ] Adicionar poucos testes end-to-end para selecionar pasta, buscar, cancelar e abrir resultado.
- [ ] Habilitar execução paralela segura do CTest.
- [ ] Configurar coverage por módulo e publicar relatório em CI.
- [ ] Definir limiares por comportamento crítico, sem perseguir cobertura cosmética.
- [ ] Criar suíte de regressão com bugs reais encontrados.
- [ ] Adicionar fuzzing para matcher, parser de ignore, encoding e paths.

## Marco 10 — Benchmarks e metas de performance (P1)

- [ ] Escolher framework de benchmark e integrar ao CMake sem afetar o build padrão.
- [ ] Criar gerador determinístico de datasets.
- [ ] Medir muitos arquivos pequenos e poucos arquivos grandes.
- [ ] Medir literal case-sensitive, case-insensitive, palavra inteira e regex/JIT.
- [ ] Medir custo de normalização Unicode opcional antes de ativá-la no caminho quente.
- [ ] Medir tempo até primeiro resultado separadamente do tempo total.
- [ ] Medir scan frio/quente e efeitos do cache do sistema operacional.
- [ ] Medir indexação inicial, incremental e troca de branch.
- [ ] Medir reuso por content hash e blob hash.
- [ ] Medir memória de filas, resultados e índice.
- [ ] Medir custo de batching e renderização da UI.
- [ ] Definir baselines por hardware/dataset e guardar resultados versionados.
- [ ] Criar alertas de regressão relevantes em CI ou execução periódica.
- [ ] Documentar metas quantitativas por classe de repositório.

## Marco 11 — Configurações, privacidade e resiliência (P2)

- [ ] Implementar configurações globais tipadas e versionadas.
- [ ] Implementar configurações por repositório com herança previsível.
- [ ] Validar limites de threads, arquivo, resultados, memória e disco.
- [ ] Adicionar importação/exportação de configurações e buscas salvas.
- [ ] Definir política de telemetria: desabilitada por padrão e somente opt-in, se existir.
- [ ] Nunca enviar nomes, caminhos ou conteúdo sem consentimento explícito.
- [ ] Proteger histórico e índice conforme permissões do usuário.
- [ ] Tratar caminhos inacessíveis, mídia removível e rede instável.
- [ ] Recuperar estado após crash sem corromper índice ou preferências.
- [ ] Implementar relatórios de crash locais e exportáveis.
- [ ] Adicionar limites contra decompression bombs e formatos especiais quando suportados.
- [ ] Fazer threat model para regex, arquivos hostis, symlinks e banco local.

## Marco 12 — CLI e extensibilidade (P2)

- [ ] Criar CLI fina sobre o mesmo `SearchService`, sem duplicar o engine.
- [ ] Suportar saída humana, JSON Lines e códigos de saída estáveis.
- [ ] Permitir busca direta, indexada e status/rebuild do índice por CLI.
- [ ] Manter cancelamento por sinal e streaming com backpressure.
- [ ] Definir interfaces para parsers de símbolos e linguagens.
- [ ] Avaliar tree-sitter para símbolos apenas atrás de adapter substituível.
- [ ] Definir API interna para novos backends de índice e watchers.
- [ ] Versionar contratos públicos antes de permitir plugins externos.
- [ ] Documentar limites de estabilidade ABI/API.

## Marco 13 — Empacotamento e releases (P2)

- [ ] Automatizar bundle Windows com Qt, runtime MinGW/MSVC e DLLs vcpkg necessárias.
- [ ] Produzir instalador Windows e avaliar MSIX versus instalador tradicional.
- [ ] Assinar executáveis e instaladores de release.
- [ ] Produzir bundle macOS, assinar e notarizar.
- [ ] Produzir AppImage e avaliar Flatpak no Linux.
- [ ] Validar instalação, atualização e desinstalação em máquinas limpas.
- [ ] Separar configurações, índice e cache para permitir upgrade/uninstall seguros.
- [ ] Definir versionamento semântico e changelog.
- [ ] Automatizar artefatos de release e checksums.
- [ ] Gerar SBOM e relatório de licenças.
- [ ] Adicionar política de atualização e canal estável/pré-release.
- [ ] Criar checklist de release, rollback e compatibilidade de banco.

## Marco 14 — Documentação profissional (P1/P2)

- [ ] Criar `docs/search-semantics.md`.
- [ ] Expandir `docs/architecture.md` com componentes e sequência de busca.
- [ ] Documentar contratos de cancelamento, ownership e threading.
- [x] Documentar schema, migrations e recuperação em `docs/storage.md`.
- [ ] Documentar formato, gerações e invalidação em `docs/indexing.md`.
- [x] Documentar branch/worktree/overlay em `docs/git-awareness.md`.
- [ ] Documentar métricas e metodologia em `docs/performance.md`.
- [ ] Documentar estados, atalhos e acessibilidade em `docs/ui.md`.
- [ ] Manter instruções de build por plataforma testadas em CI.
- [ ] Adicionar guia de troubleshooting para Qt, vcpkg, toolchains e runtime DLLs.
- [ ] Registrar decisões arquiteturais importantes como ADRs.
- [ ] Criar documentação para contribuidores e arquitetura de testes.

## Evoluções avançadas (P3)

- [ ] Busca estrutural e por símbolos com ranking por linguagem.
- [ ] Consultas compostas com operadores booleanos e filtros persistentes.
- [ ] Busca em conteúdo histórico de commits de forma opt-in.
- [ ] Comparação de resultados entre branches/worktrees.
- [ ] Índices compartilháveis somente após modelo seguro de portabilidade e privacidade.
- [ ] Preview especializado para formatos relevantes sem comprometer segurança.
- [ ] API de automação local estável.
- [ ] Avaliar aceleração SIMD e memory mapping somente com benchmarks e fallback portátil.

## Gates para considerar a versão 1.0 profissional

- [ ] Correção coberta para todas as semânticas documentadas de busca.
- [ ] Busca direta, indexada e híbrida confiáveis e canceláveis.
- [ ] Índice incremental Git-aware validado com branches, detached HEAD e worktrees.
- [ ] Nenhuma operação pesada na thread da UI.
- [ ] Orçamentos de memória, disco e filas configuráveis e testados.
- [ ] Builds reproduzíveis e CI verde nas plataformas suportadas.
- [ ] Benchmarks sem regressões críticas e metas publicadas.
- [ ] Acessibilidade, i18n, instalação, atualização e desinstalação validadas.
- [ ] Documentação, licenças, segurança e processo de release completos.
- [ ] Teste prolongado em repositórios grandes reais sem perda de resultados ou corrupção do índice.
