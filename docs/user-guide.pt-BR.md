# Guia do usuário

[English](user-guide.md)

O Uburu é uma aplicação local de busca em arquivos para quem precisa encontrar informações rapidamente em pastas, repositórios, anotações, documentos e coleções mistas de arquivos. Ele pesquisa na sua máquina, mostra resultados progressivamente e mantém a interface responsiva enquanto o trabalho está em andamento.

## Tela principal

A tela principal possui quatro áreas centrais:

- o cabeçalho de busca, onde você digita a consulta e escolhe opções de pesquisa;
- o campo de escopo, onde você escolhe a pasta ou repositório a pesquisar;
- a lista de resultados, onde aparecem arquivos e ocorrências encontradas;
- o painel de preview, onde o Uburu mostra a ocorrência selecionada com contexto.

O rodapé mostra o status atual, progresso de busca/indexação, erros parciais e informações do índice.

## Escolhendo onde pesquisar

Use o campo de escopo para escolher a pasta ou repositório que o Uburu deve pesquisar. Você pode digitar um caminho manualmente, pressionar Enter ou usar o ícone de pasta para selecionar um diretório pelo seletor do sistema.

O dropdown do escopo mostra caminhos favoritos e recentes. Selecionar um caminho altera o escopo atual. O botão de estrela adiciona ou remove o caminho dos favoritos.

O Uburu também pode incluir ou ignorar subdiretórios dentro do escopo atual. Quando você adiciona pastas incluídas ou ignoradas, o campo de escopo resume essas escolhas com `+` e `-`, por exemplo:

```text
C:\Users\voce\Documents\Projeto (+C:\Users\voce\Documents\Projeto\importante,-C:\Users\voce\Documents\Projeto\node_modules)
```

Uma pasta incluída restringe ou estende a área pesquisada conforme as regras do escopo selecionado. Uma pasta ignorada é pulada mesmo quando está dentro do escopo selecionado.

## Executando uma busca

Digite o texto que deseja encontrar no campo de busca e pressione Enter ou clique em Buscar. Os resultados aparecem progressivamente; você não precisa esperar a pasta inteira terminar de ser varrida.

Por padrão, o Uburu pesquisa tanto nomes de arquivos quanto conteúdo. Isso significa que um arquivo pode aparecer porque o nome combina com a consulta, porque o conteúdo contém a consulta, ou ambos.

Use Cancelar ou Esc para interromper uma busca em andamento. Os resultados já publicados permanecem visíveis enquanto o cancelamento é processado.

## Opções de busca

Regex habilita busca por expressão regular. Use essa opção para padrões em vez de frases comuns. Se o padrão for inválido ou caro demais, o Uburu informa o erro em vez de congelar a aplicação.

Diferenciar maiúsculas faz com que letras maiúsculas e minúsculas sejam tratadas como diferentes.

Palavra inteira retorna apenas ocorrências isoladas, sem encontrar fragmentos dentro de palavras maiores.

Respeitar `.gitignore` pula arquivos e diretórios ignorados pelas regras do Git.

Incluir ocultos permite pesquisar arquivos e pastas ocultas.

Incluir binários permite considerar arquivos binários. A busca por conteúdo ainda depende de o arquivo poder ser lido ou extraído com segurança como texto.

Incluir subdiretórios controla se o Uburu pesquisa abaixo da pasta selecionada.

## Filtro de tipos

O campo Tipos limita a busca a extensões específicas. Você pode digitar valores como:

```text
pdf, docx, txt
```

Deixe o campo vazio para pesquisar todos os tipos de arquivo suportados. O filtro considera extensões de arquivo, não uma classificação interna de conteúdo.

## Resultados e preview

A lista de resultados mostra as ocorrências encontradas. Ao selecionar um resultado, o Uburu abre um preview limitado ao redor da ocorrência. O preview rola para perto da ocorrência selecionada e destaca o texto encontrado quando possível.

Você pode usar o menu de ações do arquivo para abrir o arquivo, abrir sua localização, copiar o caminho ou copiar a ocorrência selecionada. Clique duplo ou Enter abre o resultado selecionado com a aplicação padrão do sistema.

## Indexação

O Uburu pode indexar um escopo selecionado para acelerar buscas repetidas. A indexação roda fora da thread da interface e informa o progresso no rodapé. Se o índice estiver ausente, desatualizado ou em atualização, a área de status comunica esse estado.

Para repositórios Git, o Uburu acompanha a identidade do repositório/worktree, branch e HEAD, modificações locais, arquivos deletados, arquivos ignorados e conteúdo reutilizável de documentos. O objetivo é refletir os arquivos visíveis na árvore de trabalho, não apenas um commit anterior.

## Formatos com conteúdo suportado

Arquivos de texto puro são pesquisados diretamente. O Uburu também extrai texto pesquisável de vários formatos ricos com limites de segurança:

- PDF;
- DOCX, XLSX e PPTX;
- ODT, ODS e ODP;
- RTF;
- HTML e XHTML;
- arquivos de legenda SRT e VTT.

Arquivos não suportados, protegidos, inseguros, binários ou grandes demais ainda podem ser encontrados pelo nome mesmo quando o conteúdo não puder ser pesquisado. O Uburu diferencia esses casos no status de indexação/busca sempre que possível.

## Privacidade

O Uburu foi pensado como uma ferramenta local. Buscas, índices, histórico e configurações ficam na sua máquina, a menos que você exporte diagnósticos ou arquivos explicitamente. Telemetria não é habilitada por padrão.

Caminhos e conteúdos podem ser sensíveis. Revise exportações de diagnóstico antes de compartilhá-las.

## Atalhos

- `Ctrl+F`: focar o campo de busca;
- `Ctrl+O`: escolher uma pasta;
- `Enter`: executar a busca ou abrir o resultado selecionado, dependendo do foco;
- `Esc`: cancelar uma busca em andamento ou fechar uma interface temporária;
- `F4`: ir para a próxima ocorrência;
- `Shift+F4`: ir para a ocorrência anterior;
- `Ctrl+C`: copiar o caminho do resultado selecionado quando a lista estiver focada;
- `Ctrl+Shift+C`: copiar a ocorrência selecionada;
- `Ctrl+K` ou `Ctrl+Shift+P`: abrir a paleta de comandos.

## Problemas comuns

Se uma busca retorna menos resultados do que o esperado, verifique o escopo, o filtro de Tipos, o uso de `.gitignore`, a busca em arquivos ocultos e se subdiretórios estão habilitados.

Se um documento rico aparece pelo nome, mas não pelo conteúdo, o arquivo pode estar protegido, malformado, não suportado, grande demais ou bloqueado por limites de segurança.

Se os resultados incluem arquivos fora da pasta esperada, limpe o escopo e selecione-o novamente. O escopo ativo mostrado no campo de escopo é a fonte de verdade para a próxima busca.

Se o app parece ocupado, observe o rodapé. Busca e indexação podem demorar em diretórios grandes, mas a interface deve permanecer responsiva e cancelável.
