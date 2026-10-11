# Reposco repository rules

Reposco is Windows-first and offline. This tree includes initial opt-in encrypted text history; the full v0.1.0 product remains incomplete. [Architecture](docs/architecture/overview.md) owns boundaries; [build and test](docs/development/build-and-test.md) owns commands; [threat model](docs/security/threat-model.md) owns security limitations.

- Portable core has no Win32, storage or presentation dependencies. Storage owns SQLCipher on a dedicated worker. Windows owns native APIs. Qt/QML owns presentation; QWindowKit stays in UI infrastructure.
- Use synthetic fixtures. Never log payloads or keys. No plaintext database fallback, implicit network preview loads, service locator or unbounded worker queues.
- C++20, PascalCase types/enums, camelCase functions/signals, private snake_case_ members, kPascalCase constants. Explicit ownership and RAII; Qt parent ownership is allowed. No cross-thread QObject calls or blocking I/O on the GUI thread.
- clang-format 19.1.7, 100 columns, LLVM style; Qt 6.12.0 qmlformat. Comments explain non-obvious contracts, lifetime or safety. API comments describe caller-visible behavior. English source documentation uses ASCII punctuation. No development provenance in source comments.
- Preserve existing line endings; new text follows .gitattributes. Scratch plans/logs stay under ignored .workspace or the session scratch directory.
- Dependencies are immutable recursive submodules. Update the manifest, provenance, licenses and affected tests together. Do not modify upstream working trees or share build directories with other projects.
- Run targeted tests while editing, `./tools/dev.ps1 quick` at functional checkpoints and `./tools/dev.ps1 full` once before integrated completion. Missing tools and empty suites fail. Report skipped checks and limitations honestly.
- No mandatory hooks, automatic hook installation, core.hooksPath changes, custom build orchestrator or PR-per-task policy. Use one integration branch and meaningful checkpoints. Preserve user changes and shared SDKs/caches.
- Publication, release tags and public repository creation require explicit authorization. Preparing packages does not authorize publishing.
