# Threat model

This development slice implements opt-in encrypted Unicode text history, not the full shipping product. First launch is paused. Tests and package smoke use synthetic data without collecting user clipboard contents.

The intended asset is persisted history/preview content. Pinned SQLCipher uses OpenSSL 3. Nonempty keys precede protected reads. Wrong keys/unreadable files/missing cipher identity fail closed. Tests reject no-key encrypted reads and plaintext SQLite headers. User-scope DPAPI rejects corrupted envelopes and never uses machine scope.

Same-user/elevated malware, clipboard readers, paste destinations, screen capture, pagefiles and crash dumps are outside this boundary. DPAPI is profile-bound, not portable recovery. SSD secure deletion is not promised.

Capture runs on a dedicated STA. Exclusion/ignore markers and zero or malformed local-history permission reject content before text reads. Cloud-only opt-out is not local exclusion. Text is size-bounded, validated, normalized only for identity and transferred with one outstanding payload plus activation generations. Native delayed rendering can block its worker and delay orderly Quit; forced thread termination is not used. Native executable-path exclusions are supported with bounded rules; no guarantee of detecting unmarked secrets exists.

The worker creates a 32-byte BCrypt key and atomically persists a versioned user-scope DPAPI envelope. Transient key buffers are wiped; SQLCipher memory-security mode is enabled. A writer lock prevents concurrent initialization. An incomplete key/database pair, DPAPI failure, unknown schema or storage error locks capture without automatic replacement. Data-directory access relies on Windows per-user profile permissions; this is not an additional boundary against the same user. A newly created envelope is preserved if database initialization fails, requiring deliberate recovery rather than silent reset.

The database uses DELETE journaling, FULL synchronization, secure_delete and in-memory temporary storage. WAL is not approved. Retention/count/logical-byte eviction is transactional; physical file size and SSD secure deletion are not guaranteed. Disk-full fault injection, recovery UX, richer payload artifacts and full hardening remain future work.

QML receives bounded PlainText summaries, never arbitrary image/resource markup. Future rich previews must block implicit network/file/UNC reads. Portable-core ASan/UBSan tests do not cover the Windows desktop or crypto stack. No security audit, code-signing identity, recovery guarantee or verified reporting channel exists yet.

Exact six-ASCII-digit text is excluded by default after Unicode whitespace trimming,
before UTF-8 identity/hash construction; optional eight-digit exclusion is separate.
Users may disable these rules; they also exclude legitimate standalone numbers.
No capture rule clears or writes the OS clipboard. Explicit text replay does replace it, with exclusion markers to prevent recapture and OS history/cloud admission. Existing stored secrets are not removed
by a later clipboard clear or a policy change.

Configured native application exclusions match full executable paths, case-insensitively,
from the clipboard owner's process before payload reads. With rules configured,
missing/unreadable owner provenance fails closed. Paths are bounded to 32 entries
of 512 characters. Owner identity is not an authenticated content provenance claim:
browser extensions cannot be distinguished reliably and unmarked passwords remain
capturable. No broad browser exclusion or entropy-based password detector is used.

Confirmed history clearing pauses capture, invalidates queued generations and deletes
all original/canonical/fingerprint/metadata rows transactionally with secure_delete
in the existing encrypted DELETE-journal database. No WAL is enabled. Keys and
opt-in settings are retained; capture stays paused. UI summaries are dropped and
SQLCipher page memory is released where supported. Logical deletion and releasing
application caches do not guarantee erasure of process-memory copies, pagefiles,
backups, crash dumps or physical SSD cells.

Explicit replay retrieves only the selected full original on the storage worker and writes
on the clipboard STA. Selection, search, clear, shutdown and dismiss invalidate queued
replies. A write already inside its locked publication may complete; no clipboard rollback
or backup is attempted. Automatic paste refuses unavailable/elevated targets, intervening
focus changes, modifier keys still held or a changed clipboard sequence. HWND/process/thread
checks are best-effort: HWND reuse within the same process/thread and a change between the
last Win32 check and SendInput are not atomic identity guarantees. SendInput success means
input was dispatched, not that every application accepted the paste. Clipboard locks fail
without clearing the existing content. An allocation/publication failure after EmptyClipboard
may leave marker-only content; previous clipboard values are deliberately not copied for recovery.
