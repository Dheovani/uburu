# Storage

SQLite armazenará catálogos de repositórios, worktrees, caminhos e gerações do índice, além de preferências, histórico e métricas. Conteúdo indexado será referenciado por hash; caminhos apontam para documentos e não constituem sua identidade.

O backend deverá usar transações curtas, WAL, migrações versionadas e prepared statements. Escritas de uma nova geração serão atômicas. FTS5 pode acelerar partes da consulta, mas ficará atrás de uma interface substituível e não será a única representação do índice.
