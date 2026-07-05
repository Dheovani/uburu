# Security Policy

## Scope

Uburu processes local files, repository metadata, and, in the future, persistent indexes. For that reason, security and privacy are product requirements, not just operational details.

## Reporting vulnerabilities

Until the project has a definitive public channel, report vulnerabilities privately to the maintainers. Do not open a public issue with exploitable details before initial triage.

When possible, include:

- affected version or commit;
- operating system;
- minimal reproduction steps;
- expected impact;
- synthetic files or repositories that reproduce the issue without exposing real data.

## Initial guidelines

- Do not send paths, file names, or content to external services without explicit consent.
- Do not log file content by default.
- Treat regex, encodings, symlinks, huge files, and hostile repositories as risk surfaces.
- Crashes, local denial of service, index corruption, or data exposure bugs should be treated as relevant.
