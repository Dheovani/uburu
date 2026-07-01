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
