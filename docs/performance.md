# Performance

Tempo até o primeiro resultado é a métrica principal. A busca direta transmite resultados enquanto lê arquivos linha a linha e não espera a varredura terminar. O envio à UI deve evoluir para batches adaptativos para reduzir overhead sem piorar a latência inicial.

## Métricas mínimas

- tempo de scan e tempo total;
- tempo até o primeiro resultado;
- arquivos e bytes por segundo;
- tempo de matching;
- arquivos ocultos filtrados pelo scanner;
- arquivos ignorados por `.gitignore`, `.git/info/exclude` ou ignores globais configurados;
- arquivos binários detectados pelo leitor de texto e arquivos binários efetivamente pulados;
- pico aproximado de memória;
- documentos indexados e reutilizados por hash.

No estado atual, `SearchSummary::metrics` agrega contadores básicos da busca direta. O scanner
incrementa arquivos ocultos e ignorados quando descarta entradas antes de publicá-las ao engine. O
leitor de texto identifica binários por amostragem, e o engine registra esses arquivos como binários
pulados quando `SearchOptions::includeBinary` não permite leitura textual. Diretórios ignorados
podem impedir a enumeração de descendentes; por isso os contadores representam arquivos observados
diretamente durante a varredura, não uma estimativa recursiva de tudo que havia sob o diretório.

`SearchService::searchWithEvents()` mede `timeToFirstResult` e `totalTime` no nível da estratégia de
busca selecionada. `StructuredMetricsSink` é o primeiro `MetricsSink` concreto: ele grava métricas de
busca como evento estruturado de categoria `search`, com campos numéricos para tempos, arquivos, bytes e
resultados. O logger estruturado mascara campos marcados como sensíveis por padrão; caminhos completos,
conteúdo de linhas e expressões potencialmente privadas não devem ser adicionados como campos públicos sem
uma decisão explícita da camada de aplicação.

O scanner futuro usará pool limitado, priorização de arquivos pequenos e backpressure. Otimizações deverão vir acompanhadas de benchmarks reproduzíveis para muitos arquivos pequenos, poucos arquivos grandes, literal, regex, indexação inicial e reconciliação incremental.

## Leitura de arquivos grandes

O leitor de texto processa conteúdo em blocos e mantém apenas a linha corrente, bytes pendentes de
decoding e o contexto configurado. Isso evita alocação proporcional ao tamanho total do arquivo para
UTF-8, Latin-1 e UTF-16.

`SearchOptions::maximumLineLength` limita linhas extremas para impedir crescimento não controlado
de memória. `binarySampleSize` controla a amostragem usada para detectar binários antes do matching.

Quando `contextAfterLines` é maior que zero, resultados podem ser retidos por algumas linhas para
preencher contexto posterior. O custo de memória é proporcional ao número de resultados pendentes e
ao contexto configurado, não ao tamanho total do arquivo.
