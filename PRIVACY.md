# Privacy

Reposco stores history locally and has no telemetry, account, sync, updater or crash-upload service. Reposco-owned code makes no network requests. History previews are plain text; copied URLs, HTML and Markdown are never fetched or rendered as resources.

First launch starts paused. Start / resume text capture is explicit consent to collect future Unicode text changes. The capture setting persists across launches, including Pause. Neither launch nor Resume imports the already-present clipboard. Pause rejects pending captures; an already-started storage transaction may finish. The clipboard is never changed or cleared by this slice.

On launch Reposco creates or opens history.db and a versioned user-scope DPAPI key envelope, key.dat, under its Windows per-user application data location, even while capture is paused. A separate settings.ini stores only the capture preference. Database content, comparison text and summaries stay in SQLCipher. Keys are randomly generated and transient key buffers are wiped. Key loss may make history unrecoverable; no recovery/export feature exists.

Known exclusion/ignore markers and local-history opt-out are screened before text access. Cloud-sync opt-out alone does not exclude local history. Executable-path exclusions and reliable detection of unmarked passwords are not implemented. Do not enable capture while copying unmarked secrets. History uses 1000-item, 90-day and 1-GiB original payload limits. Canonical comparison text, metadata, encryption overhead and unused pages are additional disk usage; the physical database may remain larger after eviction. There is no Clear History UI yet.

Tests use synthetic data and temporary directories. Default tests never actively read or write the session clipboard; optional active native tests require a separate private window station and fail when isolation is unavailable. The packaged --smoke path uses temporary storage with capture disabled.

Encryption does not protect against same-user malware, administrators, system clipboard readers, destination applications, pagefiles or crash dumps. See the [threat model](docs/security/threat-model.md).
