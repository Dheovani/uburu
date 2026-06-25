# Performance

Tempo até o primeiro resultado é a métrica principal. A busca direta transmite resultados enquanto lê arquivos linha a linha e não espera a varredura terminar. O envio à UI deve evoluir para batches adaptativos para reduzir overhead sem piorar a latência inicial.

## Métricas mínimas

- tempo de scan e tempo total;
- tempo até o primeiro resultado;
- arquivos e bytes por segundo;
- tempo de matching;
- arquivos binários e ignorados;
- pico aproximado de memória;
- documentos indexados e reutilizados por hash.

O scanner futuro usará pool limitado, priorização de arquivos pequenos e backpressure. Otimizações deverão vir acompanhadas de benchmarks reproduzíveis para muitos arquivos pequenos, poucos arquivos grandes, literal, regex, indexação inicial e reconciliação incremental.

## Leitura de arquivos grandes

O leitor de texto processa conteúdo em blocos e mantém apenas a linha corrente, bytes pendentes de
decoding e o contexto configurado. Isso evita alocação proporcional ao tamanho total do arquivo para
UTF-8, Latin-1 e UTF-16.

`SearchOptions::maximum_line_length` limita linhas extremas para impedir crescimento não controlado
de memória. `binary_sample_size` controla a amostragem usada para detectar binários antes do matching.

Quando `context_after_lines` é maior que zero, resultados podem ser retidos por algumas linhas para
preencher contexto posterior. O custo de memória é proporcional ao número de resultados pendentes e
ao contexto configurado, não ao tamanho total do arquivo.
