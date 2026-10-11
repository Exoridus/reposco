# Threat model

This development slice implements opt-in encrypted Unicode text history, not the full shipping product. First launch is paused. Tests and package smoke use synthetic data without collecting user clipboard contents.

The intended asset is persisted history/preview content. Pinned SQLCipher uses OpenSSL 3. Nonempty keys precede protected reads. Wrong keys/unreadable files/missing cipher identity fail closed. Tests reject no-key encrypted reads and plaintext SQLite headers. User-scope DPAPI rejects corrupted envelopes and never uses machine scope.

Same-user/elevated malware, clipboard readers, paste destinations, screen capture, pagefiles and crash dumps are outside this boundary. DPAPI is profile-bound, not portable recovery. SSD secure deletion is not promised.

Capture runs on a dedicated STA. Exclusion/ignore markers and zero or malformed local-history permission reject content before text reads. Cloud-only opt-out is not local exclusion. Text is size-bounded, validated, normalized only for identity and transferred with one outstanding payload plus activation generations. Native delayed rendering can block its worker and delay orderly Quit; forced thread termination is not used. No guarantee of detecting unmarked secrets or executable-path exclusions exists.

The worker creates a 32-byte BCrypt key and atomically persists a versioned user-scope DPAPI envelope. Transient key buffers are wiped; SQLCipher memory-security mode is enabled. A writer lock prevents concurrent initialization. An incomplete key/database pair, DPAPI failure, unknown schema or storage error locks capture without automatic replacement. Data-directory access relies on Windows per-user profile permissions; this is not an additional boundary against the same user. A newly created envelope is preserved if database initialization fails, requiring deliberate recovery rather than silent reset.

The database uses DELETE journaling, FULL synchronization, secure_delete and in-memory temporary storage. WAL is not approved. Retention/count/logical-byte eviction is transactional; physical file size and SSD secure deletion are not guaranteed. Disk-full fault injection, recovery UX, richer payload artifacts and full hardening remain future work.

QML receives bounded PlainText summaries, never arbitrary image/resource markup. Future rich previews must block implicit network/file/UNC reads. Portable-core ASan/UBSan tests do not cover the Windows desktop or crypto stack. No security audit, code-signing identity, recovery guarantee or verified reporting channel exists yet.
