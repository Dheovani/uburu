# Licenças e redistribuição

Este documento registra a política inicial de licenças do Uburu. Ele não substitui revisão jurídica
antes de distribuir builds públicos, mas evita que o projeto trate licenças como uma surpresa tardia.

## Licença do projeto

O código próprio do Uburu é distribuído sob a licença MIT, conforme o arquivo `LICENSE` na raiz do
repositório.

## Dependências principais

| Dependência | Uso no projeto | Observação inicial |
| --- | --- | --- |
| Qt 6 | Interface desktop Qt Quick/QML | Verificar módulos usados e modo de licenciamento antes de distribuir. |
| Catch2 | Testes automatizados | Dependência de desenvolvimento. |
| SQLite | Persistência local | Pode ser fornecido pelo sistema ou vcpkg. |
| PCRE2 | Regex com JIT quando disponível | Deve permanecer opcional no core. |
| libgit2 | Integração Git futura | Deve permanecer atrás de adapter explícito. |

## Qt

O Qt é disponibilizado com opções comerciais e open source. A documentação oficial do Qt informa que:

- licenças comerciais são indicadas quando o projeto não quer ou não pode cumprir os termos da
  LGPL/GPL;
- Qt sob LGPLv3 pode ser apropriado quando todos os requisitos da LGPLv3 forem cumpridos;
- alguns módulos Qt open source são GPL-only e exigem cuidado especial;
- Qt inclui código de terceiros com licenças próprias;
- Qt 6.8+ publica SBOMs para componentes de terceiros.

Política inicial do Uburu:

1. Usar apenas módulos Qt necessários à aplicação desktop.
2. Evitar módulos GPL-only salvo decisão explícita.
3. Preferir ligação dinâmica com Qt nas builds redistribuíveis.
4. Distribuir avisos de licença e textos exigidos junto dos artefatos.
5. Registrar a versão do Qt e os módulos empacotados em cada release.
6. Revisar obrigações de LGPL/comercial antes de publicar instaladores.

Fontes oficiais:

- https://doc.qt.io/qt-6/licensing.html
- https://www.qt.io/development/open-source-lgpl-obligations

## Checklist para releases

Antes de publicar um release:

- gerar lista de DLLs/bibliotecas empacotadas;
- registrar versões e licenças das dependências;
- incluir textos de licença exigidos por Qt e demais dependências;
- revisar se algum módulo Qt usado é GPL-only;
- gerar SBOM quando o pipeline de release estiver disponível;
- validar se o modo de distribuição permite cumprir LGPLv3 ou usar licença comercial.
