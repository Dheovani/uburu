# Interface

A UI Qt Quick é cliente da camada `app`. Ela contém somente apresentação e estado de interação. `SearchController` executa o serviço fora da thread gráfica, recebe ocorrências progressivas por queued connections e expõe um modelo para QML.

Textos visíveis usam `qsTr` ou `tr`, com catálogos `pt-BR` e `en-US`. A evolução deve acrescentar batching de resultados, preview contextual com highlight, filtros avançados, histórico, status do índice e diagnóstico sem mover regras para QML.

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
