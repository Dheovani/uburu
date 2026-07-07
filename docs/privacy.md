# Privacy and telemetry

Uburu is a local-first desktop search tool. Search expressions, file names, directory names, file contents, Git metadata, index contents, logs, and diagnostics must remain local unless the user explicitly exports or shares them. The application currently has no telemetry transport, no background upload path, and no remote diagnostics endpoint.

Telemetry is disabled by default. `GlobalSettings::telemetryEnabled` is the only typed opt-in flag at this stage, and repository settings cannot enable telemetry independently. If telemetry is ever implemented, it must remain opt-in, must explain what is collected before activation, must provide a direct disable path, and must never include names, paths, content, search expressions, Git branch names, blob identifiers tied to private repositories, or index records without separate explicit consent.

Diagnostics are local developer artifacts. Structured logs, tracing events, and diagnostic reports mask fields marked as sensitive by default. Sensitive fields may include paths, expressions, line text, preview content, repository identifiers, and user-controlled labels. Exporting a diagnostic report writes a local file selected by the application or user; uploading or sending that file is outside the current product contract and must require an explicit user action.

Saved searches, recent paths, favorites, settings, and the persistent index are local user data. They should be stored under the user's profile/application data locations with normal operating-system permissions, and future import/export flows must make the privacy boundary visible. Exported settings should separate harmless preferences from history-like data so users can share configuration without accidentally sharing private project names or search terms.

The storage layer creates local state directories through the private-storage helper. POSIX builds restrict those directories to the current user, while Windows builds rely on the user's profile ACLs. This protection is not encryption and does not replace full-disk encryption, but it prevents Uburu from deliberately creating world-readable index or history directories.

Crash reporting must start as a local exportable artifact, not an automatic upload. A crash report may include application version, platform, build configuration, active feature flags, sanitized diagnostics, and recent non-sensitive error categories. It must not include file contents, full paths, search expressions, or index records unless a future crash-report UI asks for that data explicitly and labels it as sensitive.

Threat modeling for hostile files, regex patterns, symlinks, and the local database belongs to Milestone 11. Until that model is complete, privacy-sensitive changes should preserve the conservative default: local processing, masked diagnostics, no automatic network transmission, and explicit user consent before exporting anything that may identify a project or user.
