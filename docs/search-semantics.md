# Semântica de busca

Este documento define o contrato observável da busca direta do Uburu. A busca indexada deve
preservar a mesma semântica para que os resultados não mudem quando a fonte passar de leitura direta
para índice persistente.

## Unidade de resultado

Um resultado representa uma ocorrência individual, não apenas uma linha que contém a expressão.

Cada ocorrência deve conter:

- caminho relativo ao diretório pesquisado;
- linha em base 1;
- coluna visual em base 1;
- tamanho da ocorrência em bytes UTF-8;
- texto da linha para preview;
- spans de highlight da linha;
- contexto anterior/posterior quando configurado.

Se uma linha contém mais de uma ocorrência, cada ocorrência é publicada como um `SearchResult`
separado.

## Escopo de busca

`SearchQuery` possui um `SearchScope` com zero ou mais `SearchRoot`. Cada root representa uma raiz
física pesquisável e pode ter diretórios incluídos/excluídos próprios. Isso permite pesquisar
simultaneamente em múltiplos repositórios ou diretórios avulsos e ignorar subárvores específicas, como
`node_modules`, por raiz.

`SearchQuery::root` permanece como compatibilidade temporária para chamadas antigas com raiz única. A
busca direta resolve roots efetivos assim:

1. se `SearchScope::roots` não está vazio, usa esses roots;
2. caso contrário, usa `SearchQuery::root` com os filtros globais de `SearchOptions`.

`SearchResult::path` continua relativo ao root que produziu o resultado. Para desambiguar resultados
com o mesmo caminho relativo em roots diferentes, `SearchResult::search_root` carrega a raiz física de
origem.

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
- `regex_compile_failed`;
- `regex_resource_limit_exceeded`;
- `regex_timeout`;
- `invalid_regex_limit`;
- `invalid_result_limit`;
- `invalid_per_file_result_limit`;
- `invalid_maximum_file_size`;
- `file_open_failed`;
- `file_read_failed`.

O modo regex é reportado como `unsupported_search_mode` quando o build não possui PCRE2. Quando
PCRE2 está disponível, erros de compilação retornam `regex_compile_failed` com mensagem e offset
fornecidos pelo backend.

## Busca literal

A busca literal interpreta a expressão como texto comum. Caracteres com significado especial em
regex não possuem significado especial nesse modo.

Por padrão, a busca literal é case-insensitive. Quando `case_sensitive` está habilitado, os code
points UTF-8 da linha e da expressão devem corresponder exatamente.

A comparação case-insensitive usa case folding Unicode simples, limitado a transformações de um code
point para um code point. Isso cobre ASCII e letras latinas pré-compostas comuns, como `AÇÃO` contra
`ação` e `CAFÉ` contra `café`.

Ainda não há normalização canônica. Portanto, `é` pré-composto e `e` + acento combinante podem ser
tratados como sequências diferentes até introduzirmos uma etapa opcional de normalização com custo
medido.

O matcher trabalha sobre texto UTF-8 normalizado pelo leitor de texto. Offsets e tamanhos internos de
match permanecem em bytes UTF-8; `SearchResult::column` e `MatchSpan::column` são colunas visuais em
base 1 calculadas por code point UTF-8.

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

`whole_word` usa limites de palavra para texto natural. Letras ASCII, dígitos ASCII e letras latinas
pré-compostas são considerados parte de uma palavra. Pontuação e `_` são limites de palavra nesse
modo.

Exemplos:

- `ação` casa em `pré-ação`;
- `ação` não casa em `préação`;
- `search` casa em `search_engine`, porque `_` é pontuação para texto natural.

Para código-fonte, `whole_identifier` usa limites de identificador. Letras ASCII, dígitos ASCII e `_`
são considerados parte do identificador.

Exemplos:

- `search` não casa em `search_engine`;
- `search` não casa em `searchEngine`;
- `search` não casa em `search2`;
- `search` casa em `call(search)`.

Se `whole_word` e `whole_identifier` forem habilitados ao mesmo tempo, a ocorrência precisa satisfazer
as duas regras de boundary.

## Regex

Quando `SearchOptions::mode` estiver em `SearchMode::regex`, a expressão é compilada uma vez por
busca com PCRE2 em modo UTF/UCP. A busca reutiliza a regex compilada em todas as linhas processadas,
evitando recompilar o padrão dentro do loop de arquivos.

Por padrão, regex também respeita `case_sensitive`: quando desabilitado, o padrão é compilado com
`PCRE2_CASELESS`.

O matcher tenta habilitar PCRE2 JIT com `PCRE2_JIT_COMPLETE`. Quando JIT é aceito, o resumo da busca
registra `RegexExecutionMode::jit`. Quando PCRE2 está disponível mas JIT não é aceito para o padrão
ou build corrente, a busca continua com fallback interpretado e registra
`RegexExecutionMode::interpreted_fallback`.

Regex possui limites configuráveis em `SearchOptions`:

- `regex_match_limit`;
- `regex_depth_limit`;
- `regex_heap_limit_kib`;
- `regex_timeout`.

Os três primeiros são aplicados no `pcre2_match_context`. O timeout usa callouts automáticos do PCRE2
e interrompe a tentativa de match quando o orçamento de tempo expira. Quando um limite é atingido, a
busca para e retorna erro tipado, distinguindo `regex_resource_limit_exceeded` de `regex_timeout`.

Regex preserva a mesma unidade de resultado da busca literal: cada ocorrência vira um resultado
individual, com offset e tamanho em bytes UTF-8. `whole_word` e `whole_identifier` também são
aplicados sobre matches regex.

Erros de compilação retornam `regex_compile_failed` com:

- `translation_key`, para a UI traduzir a mensagem visível;
- `context`, com a mensagem técnica fornecida pelo backend;
- `offset`, quando o PCRE2 informa a posição do erro no padrão.

## Alvo da busca

`SearchOptions::target` define onde a expressão será aplicada:

- `content`: busca apenas no conteúdo dos arquivos;
- `file_name`: busca apenas no caminho relativo do arquivo;
- `content_and_file_name`: publica ocorrências tanto no caminho relativo quanto no conteúdo.

Resultados de nome de arquivo usam `SearchResultKind::file_name`, linha `0` e coluna em base 1 dentro
do caminho relativo. Resultados de conteúdo usam `SearchResultKind::content` e linha em base 1.

A busca por nome de arquivo não abre o arquivo, permitindo encontrar caminhos mesmo quando o conteúdo
não está acessível naquele momento. Regex, case sensitivity e regras de palavra inteira também se
aplicam ao caminho relativo.

## Filtros de arquivos

O scanner aplica filtros antes de entregar `FileEntry` ao motor de busca:

- tamanho máximo;
- extensões permitidas;
- diretórios incluídos;
- diretórios excluídos;
- globs incluídos;
- globs excluídos;
- arquivos ocultos, conforme `include_hidden`.

Exclusões têm precedência sobre inclusões. Portanto, um arquivo dentro de um diretório incluído ainda
será ignorado se também cair em um diretório excluído ou glob excluído.

Extensões são comparadas sem o ponto inicial. No Windows, a comparação de extensões e globs é
case-insensitive; nas demais plataformas, a comparação preserva a sensibilidade a maiúsculas e
minúsculas do sistema.

Os globs iniciais suportam `*` e `?` sobre o caminho relativo normalizado com `/`. Eles são uma
semântica simples de filtro do Marco 1, não uma implementação completa de `.gitignore`.

## Limites de resultados

`result_limit` é um limite global da busca. Um resultado só pode ser publicado se ainda estiver dentro
do limite.

Quando o limite é atingido:

- nenhum resultado adicional deve ser emitido;
- `SearchSummary::limit_reached` deve ser `true`;
- `SearchSummary::matches` deve contar apenas os resultados publicados.

`per_file_result_limit` limita a quantidade de resultados publicados para um mesmo arquivo. Quando o
limite por arquivo é atingido:

- a busca para de publicar ocorrências daquele arquivo;
- a varredura continua nos próximos arquivos;
- `SearchSummary::files_with_match_limit_reached` é incrementado;
- `SearchSummary::limit_reached` permanece reservado para o limite global.

## Ordem determinística

A busca direta publica arquivos em ordem determinística de caminho. O scanner ordena as entradas de
cada diretório pelo caminho normalizado antes de visitar arquivos e subdiretórios.

A estratégia inicial de relevância é deliberadamente simples: em buscas combinadas, ocorrências no
nome/caminho relativo são publicadas antes das ocorrências de conteúdo do mesmo arquivo. Ranking mais
sofisticado deve ser introduzido depois com métricas e sem quebrar a estabilidade da ordem final.

## Resultados progressivos

A busca direta publica resultados assim que cada ocorrência é encontrada. Ela não espera a varredura
inteira terminar para entregar o primeiro resultado ao consumidor. A ordenação determinística é feita
por diretório, não por materialização antecipada da árvore inteira.

Quando `context_after_lines` é maior que zero, resultados de conteúdo podem ser retidos por até esse
número de linhas para preencher o contexto posterior. Esse atraso é local ao arquivo e não exige
carregar o arquivo inteiro.

## Encoding, binários e linhas

O leitor de texto do core detecta BOM e suporta:

- UTF-8 com ou sem BOM;
- UTF-16 LE com BOM;
- UTF-16 BE com BOM;
- fallback configurável para Latin-1 ou UTF-8 sem BOM.

UTF-8 inválido segue `SearchOptions::invalid_utf8_policy`: substituir por U+FFFD, ignorar o byte
inválido ou falhar a leitura. A política padrão substitui sequências inválidas para preservar busca
em arquivos parcialmente corrompidos sem abortar todo o diretório.

Arquivos binários são detectados por amostra configurável antes da leitura linha a linha. A detecção
considera byte NUL e proporção de bytes de controle, mas não classifica UTF-16 com BOM como binário
apenas por conter NULs alternados.

## Finais de linha

A leitura linha a linha suporta `LF`, `CRLF`, `CR` isolado, linhas vazias e arquivo sem newline final.
Os marcadores de fim de linha não fazem parte de `SearchResult::line_text`, e o leitor registra o tipo
de final de linha em `TextLine` para uso futuro por preview, offsets e diagnósticos.

## Cancelamento e falhas parciais

Cancelamento é cooperativo. A busca deve parar assim que possível depois que o token de cancelamento
for sinalizado.

O resumo distingue explicitamente:

- conclusão normal;
- cancelamento;
- limite atingido;
- falha parcial de leitura;
- query inválida.

Falhas ao abrir ou ler um arquivo não interrompem silenciosamente toda a busca. O resumo marca
`partial_failure`, incrementa `files_with_read_errors` e adiciona um erro tipado com o caminho relativo
no contexto. Resultados já publicados continuam válidos.

Falhas de permissão, arquivos removidos entre scan e leitura, e falhas de stream são normalizadas como
falhas parciais tipadas. O erro concreto pode variar por plataforma, mas a busca não deve descartá-lo
silenciosamente.

## Arquivos alterados durante a busca

A busca direta representa o estado observado no momento em que cada arquivo é aberto. Se um arquivo
for alterado entre o scan e a leitura, o conteúdo lido depois da abertura é a fonte da verdade para
aquela ocorrência.

Se o arquivo for removido, ficar inacessível ou falhar durante a leitura, a busca registra falha
parcial tipada e continua nos demais arquivos. O Marco 1 não tenta criar snapshots consistentes da
árvore inteira; essa garantia pertence ao desenho futuro de índice, overlay e integração com Git.

## Ownership e cópias

`SearchResult` possui o texto publicado para garantir segurança quando o consumidor processar
resultados de forma assíncrona. O engine evita cópias intermediárias no caminho quente sempre que não
precisa transferir ownership, mas materializa a linha/caminho ao construir o resultado publicado.

## Comportamentos ainda pendentes no Marco 1

Esta primeira versão do documento registra o contrato inicial e as lacunas conhecidas. Ainda faltam:

- erros traduzíveis para query inválida;
- filtros por nome, glob, extensão, diretório e tamanho;
- ordenação determinística formal;
- estratégia inicial de relevância.
