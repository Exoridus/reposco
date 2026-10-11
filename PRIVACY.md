# Privacy

The engineering preview does not read, monitor, store or modify the user's clipboard. It displays fixed text. There is no telemetry, account, sync, updater or crash-upload service. Reposco-owned code makes no network requests.

Tests create synthetic encrypted databases in temporary directories and protect synthetic keys with user-scope DPAPI. Temporary files are removed through RAII. No real history or persistent user key is created.

Future history requires explicit capture consent, exclusions, encrypted storage and safe retention. These are requirements, not implemented guarantees. Encryption will not protect against same-user malware, administrators, system clipboard readers, destination applications, pagefiles or crash dumps. See the [threat model](docs/security/threat-model.md).
