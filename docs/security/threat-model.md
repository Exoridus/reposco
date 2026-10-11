# Threat model

This is a synthetic integration proof, not a shipping clipboard product. No clipboard bytes are read. Tests demonstrate primitives, not a production key lifecycle.

The intended asset is persisted history/preview content. Pinned SQLCipher uses OpenSSL 3. Nonempty keys precede protected reads. Wrong keys/unreadable files/missing cipher identity fail closed. Tests reject no-key encrypted reads and plaintext SQLite headers. User-scope DPAPI rejects corrupted envelopes and never uses machine scope.

Same-user/elevated malware, clipboard readers, paste destinations, screen capture, pagefiles and crash dumps are outside this boundary. DPAPI is profile-bound, not portable recovery. SSD secure deletion is not promised.

Before capture, implement bounded STA capture, exclusions before reads, safe replay sequencing, consent, random keys/versioned DPAPI envelopes, zeroization, transactions and locked-state recovery. Test journal/WAL/temp/index/thumbnail artifacts and corruption/disk-full. The proof uses DELETE journaling and memory temporary storage; WAL is not approved.

UI text is fixed plain text. Future rich previews must block implicit network/file/UNC reads and enforce budgets. No sanitizer, security audit, code-signing identity, recovery guarantee or verified reporting channel exists yet.
