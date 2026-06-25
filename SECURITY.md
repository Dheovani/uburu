# Política de segurança

## Escopo

O Uburu processa arquivos locais, metadados de repositórios e futuramente índices persistentes. Por
isso, segurança e privacidade são requisitos de produto, não apenas detalhes operacionais.

## Como reportar vulnerabilidades

Enquanto o projeto não tiver canal público definitivo, reporte vulnerabilidades de forma privada aos
mantenedores. Não abra issue pública com detalhes exploráveis antes de uma triagem inicial.

Inclua, quando possível:

- versão ou commit afetado;
- sistema operacional;
- passos mínimos de reprodução;
- impacto esperado;
- arquivos ou repositórios sintéticos que reproduzam o problema sem expor dados reais.

## Diretrizes iniciais

- Não envie caminhos, nomes de arquivos ou conteúdo para serviços externos sem consentimento
  explícito.
- Não registre conteúdo de arquivos em logs por padrão.
- Trate regex, encodings, symlinks, arquivos enormes e repositórios hostis como superfícies de
  risco.
- Bugs de travamento, negação de serviço local, corrupção de índice ou exposição de dados devem ser
  tratados como relevantes.
