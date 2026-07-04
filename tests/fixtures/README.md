# Fixtures

Fixtures devem permanecer pequenas, determinísticas e conter explicitamente casos UTF-8, UTF-16,
CRLF, binário, symlink e repositórios Git com worktrees.

Use `test-fixtures.hpp` para dados pequenos compartilhados por testes unitários e de integração. A
intenção é nomear semanticamente os casos recorrentes sem esconder a estrutura dos arquivos:

- texto Unicode precomposto e decomposto;
- bytes de encoding UTF-8 com BOM, UTF-16 LE/BE, Latin-1 e binário;
- layouts mínimos de `.gitignore`;
- arquivos básicos de uma worktree Git descartável.

Evite fixtures grandes ou dependentes do ambiente local. Quando o teste precisar criar arquivos
reais, combine estas fixtures com os helpers RAII de `tests/helpers`.
