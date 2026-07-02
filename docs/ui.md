# Interface

A UI Qt Quick é cliente da camada `app`. Ela contém somente apresentação e estado de interação. `SearchController` executa o serviço fora da thread gráfica, recebe ocorrências progressivas por queued connections e expõe um modelo para QML.

Textos visíveis usam `qsTr` ou `tr`, com catálogos `pt-BR` e `en-US`. A evolução deve acrescentar batching de resultados, preview contextual com highlight, filtros avançados, histórico, status do índice e diagnóstico sem mover regras para QML.

## Direção visual do Marco 8

A tela principal deve comunicar velocidade, agilidade e eficiência sem parecer genérica. A primeira
iteração do Marco 8 adota uma composição de comando técnico: topo compacto para busca e filtros,
cartões de status com linguagem de instrumentação, lista de resultados em fluxo e preview lateral.

O tema escuro inicial usa contraste controlado, azul como cor de ação e verde como acento pontual de
confirmação para remeter a telemetria, baixa latência e ferramentas técnicas modernas sem tornar a UI
excessivamente chamativa. Estados vazios devem ser claros e instrutivos, explicando o próximo passo
sem bloquear a busca progressiva.

O layout alterna para orientação vertical em larguras menores para manter a área de busca sempre
prioritária e preservar leitura confortável da lista e do preview.

A tela principal deve permanecer como composição de alto nível. Componentes reutilizáveis do QML
ficam em `apps/desktop/qml/components/`, preferencialmente com arquivos pequenos e responsabilidade
única para evitar que `main.qml` concentre toda a evolução do Marco 8.

## Métricas da busca na tela principal

O cabeçalho da busca deve expor métricas operacionais leves para reforçar a percepção de velocidade
sem transformar a tela em painel de diagnóstico. As métricas visíveis no Marco 8 são:

- resultados visíveis;
- arquivos lidos;
- tempo até o primeiro resultado;
- duração total da busca.

Esses valores são calculados pelo controller a partir do resumo retornado pelo `SearchService`. O QML
apenas apresenta propriedades observáveis; não deve medir busca, inferir progresso do core nem acessar
threads de worker diretamente.

## Lista de resultados

A lista de resultados usa `ListView` com reutilização de delegates e cache visual limitado. Isso evita
instanciar componentes QML para todos os resultados quando houver muitos itens, mantendo a renderização
proporcional à área visível e a uma pequena margem de navegação.

O modelo C++ ainda retém os resultados publicados pela busca. Otimizações futuras de memória devem
evoluir o contrato do modelo, não substituir a virtualização visual por lógica manual em QML.

## Preview de arquivo

O preview da tela principal é carregado pelo `SearchController` em worker assíncrono, usando o leitor
de texto do core. Ao selecionar outro resultado, a prévia anterior recebe cancelamento cooperativo e
eventos atrasados são descartados pelo watcher ativo.

A prévia é limitada por janela de linhas ao redor da ocorrência e por orçamento de bytes para manter a
UI responsiva. O QML exibe apenas o estado observável: arquivo selecionado, localização, texto da
prévia e indicador de carregamento.

## Interações com arquivos encontrados

Resultados devem permitir operações diretas sobre o arquivo encontrado sem quebrar a separação entre
UI e plataforma. O comportamento desejado para o Marco 8 é:

- clique secundário sobre arquivo ou resultado abre o menu de contexto nativo do sistema
  operacional, equivalente a clicar no arquivo pelo gerenciador de arquivos;
- Windows deve usar integração Shell/Explorer;
- macOS deve usar integração Finder;
- Linux deve preferir integrações do desktop/portal quando disponíveis e oferecer fallback próprio;
- a UI QML deve acionar um serviço/adaptador de plataforma, não implementar regras de shell no QML;
- quando o menu nativo não estiver disponível, o fallback deve expor ao menos abrir arquivo, abrir
  pasta, copiar caminho e copiar ocorrência.

O fallback próprio inicial do Marco 8 é oferecido pelo menu de contexto da lista de resultados. Ele
encaminha ações ao `SearchController`, que usa APIs Qt para abrir arquivos/pastas e copiar textos,
mantendo QML sem lógica de plataforma. O menu nativo do sistema operacional continua sendo uma
integração posterior, por adaptador específico.

## Formatos com extração de conteúdo pendente

A busca direta atual trata arquivos de texto puro como conteúdo pesquisável e pode encontrar arquivos
binários ou empacotados pelo nome quando o alvo combina conteúdo e nome de arquivo. Formatos como PDF,
DOCX, ODT, RTF e EPUB não devem ser apresentados como conteúdo pesquisável enquanto não houver
extratores dedicados.

A UI deve deixar essa limitação clara quando o usuário filtrar por esses tipos: o filtro seleciona os
arquivos pelo nome/extensão, mas a busca dentro do conteúdo depende de uma futura camada de extração.
Essa camada deve viver abaixo da UI, no core/text ou em adaptadores específicos, com limites de
memória, cancelamento cooperativo e tratamento de arquivos hostis.
