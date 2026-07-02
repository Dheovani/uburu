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

O controller também entrega uma versão HTML segura da prévia quando há ocorrência selecionada. Essa
representação escapa o conteúdo do arquivo, destaca todas as ocorrências conhecidas na linha ativa e
mantém números de linha alinhados em fonte monoespaçada. O `PreviewPane` preserva uma propriedade de
largura de tabulação para futura exposição em preferências sem reescrever o componente.

## Interações com arquivos encontrados

Resultados devem permitir operações diretas sobre o arquivo encontrado sem quebrar a separação entre
UI e plataforma. O comportamento desejado para o Marco 8 é um menu próprio do Uburu com ações
equivalentes às operações comuns do gerenciador de arquivos, não uma reprodução completa do menu
nativo do Explorer/Finder/desktop Linux.

O menu de contexto da lista de resultados encaminha intenções ao `SearchController`, mantendo QML sem
lógica de plataforma. As ações iniciais são abrir arquivo, abrir com quando a plataforma oferecer um
seletor de aplicativo, abrir local do arquivo, copiar caminho e copiar ocorrência. No Windows, a ação
`Abrir com...` usa o seletor do sistema operacional. Em outras plataformas, ela deve evoluir por
adaptadores específicos de Finder, portal ou desktop environment.

Visualmente, menus de ação usam o mesmo vocabulário da tela principal: superfície elevada, borda
discreta, realce azul suave no item ativo e atalhos alinhados à direita. O objetivo é parecer
integrado ao produto sem imitar integralmente o menu nativo da plataforma. Esse padrão é compartilhado
pelo menu de arquivos da lista de resultados e pelo menu de texto da pré-visualização.

Na iteração inicial, abrir arquivo usa o aplicativo padrão configurado no sistema operacional. A lista
também aceita duplo clique ou Enter para abrir o resultado selecionado, Ctrl+C para copiar o caminho
absoluto e Ctrl+Shift+C para copiar a ocorrência com localização e trecho.

## Atalhos e paleta de comandos

A tela principal expõe uma paleta de comandos inicial por `Ctrl+K` ou `Ctrl+Shift+P`. A paleta é um
componente QML de coordenação: ela lista comandos disponíveis e emite a escolha do usuário, enquanto
as ações continuam delegadas aos componentes existentes e ao `SearchController`.

Atalhos essenciais disponíveis nesta fase:

- `Ctrl+F`: focar o campo de busca;
- `Ctrl+O`: selecionar diretório ou repositório;
- `Ctrl+K` / `Ctrl+Shift+P`: abrir paleta de comandos;
- `Ctrl+D`: alternar favorito para o diretório atual;
- `Esc`: cancelar busca em andamento;
- `Enter`: executar busca ou abrir resultado selecionado conforme o foco;
- `Ctrl+C`: copiar caminho do resultado quando a lista está focada;
- `Ctrl+Shift+C`: copiar ocorrência do resultado quando a lista está focada.

A paleta deve evoluir para incluir configurações, diagnósticos, histórico, buscas salvas e navegação
entre ocorrências, sem mover regras de domínio para QML.

## Persistência de estado da janela

O estado visual da janela principal é persistido em `Settings` no QML, pois pertence à camada de
apresentação. Nesta fase, a aplicação restaura geometria da janela, tamanho preferido do painel de
resultados e filtros visuais da busca. A consulta textual em si não é restaurada automaticamente para
evitar reexecutar uma busca antiga ao abrir o aplicativo.

O último diretório selecionado é restaurado pelo `SearchController`, junto do histórico de diretórios e
favoritos já persistidos com `QSettings`. O controller só restaura um diretório recente quando ele ainda
existe no sistema de arquivos.

## Ajuda contextual

Controles potencialmente ambíguos devem expor ajuda curta e localizada por tooltip, sem transformar a
tela principal em documentação longa. O Marco 8 usa `InfoIcon` para explicações de escopo e tipos de
documento, além de tooltips diretos nos chips de filtro como regex, case-sensitive, palavra inteira,
respeitar `.gitignore` e incluir subdiretórios.

Essas mensagens são textos visíveis ao usuário e devem continuar passando por `qsTr`/catálogos de
tradução.

O chip de regex só fica habilitado quando o build expõe suporte a PCRE2 pelo `SearchController`. Mesmo
assim, o core continua sendo a autoridade final e valida `SearchQuery` para impedir regex em builds sem
backend compatível.

## Formatos com extração de conteúdo pendente

A busca direta atual trata arquivos de texto puro como conteúdo pesquisável e pode encontrar arquivos
binários ou empacotados pelo nome quando o alvo combina conteúdo e nome de arquivo. Formatos como PDF,
DOCX, ODT, RTF e EPUB não devem ser apresentados como conteúdo pesquisável enquanto não houver
extratores dedicados.

A UI deve deixar essa limitação clara quando o usuário filtrar por esses tipos: o filtro seleciona os
arquivos pelo nome/extensão, mas a busca dentro do conteúdo depende de uma futura camada de extração.
Essa camada deve viver abaixo da UI, no core/text ou em adaptadores específicos, com limites de
memória, cancelamento cooperativo e tratamento de arquivos hostis.
