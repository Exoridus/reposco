# Infrastructure reference adaptations

ExoSnap's committed next tree supplied conventions for ignores, LF attributes, editor settings, LLVM formatting, target-local warnings, optional analysis, Qt provisioning, tests and `@AGENTS.md`. Reposco uses its own 100-column style and curated clang-tidy set.

Reposco has no exo-dev orchestrator, automatic Git configuration, mandatory hooks, FFmpeg/media dependencies, recording tests, updater, cache-warming jobs or complex release pipeline. main is primary; one branch carries integration. SDKs may be reused but generated outputs are repository-local. Security contacts and privacy statements reflect only this project.

Commands live in [build and test](development/build-and-test.md), quality in [code quality](development/code-quality.md), and ownership in [architecture](architecture/overview.md).
