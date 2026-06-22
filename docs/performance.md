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
