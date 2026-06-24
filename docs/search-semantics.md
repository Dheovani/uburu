# Semântica de busca

Este documento define o contrato observável da busca direta do Uburu. A busca indexada deve
preservar a mesma semântica para que os resultados não mudem quando a fonte passar de leitura direta
para índice persistente.

## Unidade de resultado

Um resultado representa uma ocorrência individual, não apenas uma linha que contém a expressão.

Cada ocorrência deve conter:

- caminho relativo ao diretório pesquisado;
- linha em base 1;
- coluna em base 1;
- tamanho da ocorrência;
- texto da linha para preview e highlight.

Se uma linha contém mais de uma ocorrência, cada ocorrência é publicada como um `SearchResult`
separado.

## Validação da consulta

A busca valida `SearchQuery` antes de iniciar a varredura do filesystem. Consultas inválidas não
devem chamar o scanner nem publicar resultados.

Erros de validação são retornados como códigos tipados em `SearchSummary::errors`, para que UI e
serviços possam traduzir mensagens sem acoplar texto visível ao core.

Erros iniciais suportados:

- `empty_root`;
- `root_not_found`;
- `root_not_directory`;
- `empty_expression`;
- `unsupported_search_mode`;
- `invalid_result_limit`;
- `invalid_maximum_file_size`.

O modo regex é reportado como `unsupported_search_mode` até a implementação PCRE2 ficar disponível.

## Busca literal

A busca literal interpreta a expressão como texto comum. Caracteres com significado especial em
regex não possuem significado especial nesse modo.

Por padrão, a busca literal é case-insensitive. Quando `case_sensitive` está habilitado, os bytes da
linha e da expressão devem corresponder exatamente.

No estágio atual, a comparação case-insensitive é ASCII-oriented. O Marco 1 ainda deve evoluir essa
regra para Unicode consistente antes de considerar o item de case-insensitive concluído.

## Ocorrências sobrepostas

Ocorrências sobrepostas são preservadas em busca literal.

Exemplo:

```txt
texto:      aaaa
expressão: aa
colunas:   1, 2, 3
```

Esse comportamento evita esconder matches reais e mantém o contrato previsível para futuras
estratégias de highlight, ranking e índice.

## Palavra inteira

No estágio atual, `whole_word` usa uma regra ASCII-oriented: letras, dígitos e `_` são considerados
caracteres de palavra. Isso trata identificadores como `search_engine` como uma única palavra.

O Marco 1 ainda deve separar explicitamente:

- palavra inteira para texto natural com limites Unicode;
- identificador inteiro para código-fonte.

## Regex

Quando `SearchOptions::mode` estiver em `SearchMode::regex`, a expressão deve ser compilada como
regex PCRE2. O suporte ainda é pendente.

A implementação final deve:

- retornar erro de compilação com posição e mensagem traduzível;
- usar PCRE2 JIT quando disponível;
- declarar fallback quando JIT não estiver disponível;
- aplicar limites de recurso para evitar padrões patológicos;
- preservar a mesma unidade de resultado usada pela busca literal.

## Limites de resultados

`result_limit` é um limite global da busca. Um resultado só pode ser publicado se ainda estiver dentro
do limite.

Quando o limite é atingido:

- nenhum resultado adicional deve ser emitido;
- `SearchSummary::limit_reached` deve ser `true`;
- `SearchSummary::matches` deve contar apenas os resultados publicados.

## Cancelamento e falhas parciais

Cancelamento é cooperativo. A busca deve parar assim que possível depois que o token de cancelamento
for sinalizado.

O Marco 1 ainda deve distinguir explicitamente:

- conclusão normal;
- cancelamento;
- limite atingido;
- falha parcial de leitura;
- query inválida.

## Comportamentos ainda pendentes no Marco 1

Esta primeira versão do documento registra o contrato inicial e as lacunas conhecidas. Ainda faltam:

- erros traduzíveis para query inválida;
- case-insensitive Unicode consistente;
- regex com PCRE2;
- filtros por nome, glob, extensão, diretório e tamanho;
- ordenação determinística formal;
- estratégia inicial de relevância.
