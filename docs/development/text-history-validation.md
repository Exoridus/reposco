# Text-history validation

Validated locally on 2026-10-11. This extends the [bootstrap proof](bootstrap-validation.md)
with initial opt-in Unicode text history; it does not complete the v0.1.0 product.

## Implemented behavior

The Windows capture worker screens exclusion, ignore and local-history permission markers
before reading text. A single acknowledged payload and activation generations bound queued
work and reject stale captures after pause. Starting or resuming does not import the current
clipboard. First launch is paused; the capture preference persists.

The storage worker creates a random 32-byte BCrypt key, saves a versioned user-scope DPAPI
envelope atomically and owns SQLCipher access. A writer lock serializes profile initialization.
Missing keys, incomplete key/database pairs, truncated existing databases and unsupported
schemas lock capture without replacing files.

Schema version 1 stores original UTF-8 and canonical text separately. Canonicalization version
1 converts line endings, applies NFC and trims edges for a domain-separated SHA-256 identity.
Smart dedup preserves the entry ID and first-seen timestamp while updating recency and the
latest original payload. Retired IDs are not reused. Eviction is transactional: 1000 items,
90 days and 1 GiB of original payload bytes. Canonical text, metadata and unused pages consume
additional disk space. The QML list receives the latest 100 plain-text summaries, each capped
at 200 characters. Database and profile I/O run off the GUI thread.

## Executed checks

The environment and dependency revisions remain those in the bootstrap report: Qt 6.12.0,
QWindowKit 1.5.0, SQLCipher 4.19.0 and OpenSSL 3.5.9. Local Windows compilation uses MSVC
19.51.36260, not the still-unrun MSVC 2022 CI runner.

| Check | Actual result |
| --- | --- |
| `./tools/dev.ps1 format` | Passed |
| `./tools/dev.ps1 quick` | Debug build and 7/7 CTest suites passed; 8.03 seconds for tests |
| Focused desktop/QML tests after the final QML change | 2/2 passed |
| `cmake --build --preset windows-debug --target all_qmllint` | Passed without warnings after binding the delegate to its outer component |
| `./tools/quality.ps1 -Mode Fast` with the exact Qt root | Formatting, repository hygiene, dependency/licenses, CMake and workflows passed |
| clang-tidy from `quality.ps1 -Mode Analysis` | All 21 owned C++ translation units passed |
| cppcheck using the owned compile database | Passed after correcting two copies and documenting the conditional Win32 ownership model |
| `./tools/dev.ps1 build -Configuration Release` | Passed; final incremental build 5.68 seconds |
| `./tools/package.ps1` | Portable ZIP deployment and isolated runtime-path smoke passed; 42,546,182 bytes |
| WSL Linux portable core with ASan/UBSan | 1/1 passed; 3.14 seconds |

The integrated `full` invocation exposed analyzer findings and stopped as intended. After
fixing them, the affected analysis, formatting, tests, Release and packaging stages were
resumed explicitly. The failed invocation is not recorded as a successful command.

Tests use synthetic fixtures and temporary profiles. They cover marker read ordering,
UTF-16 validation, original preservation, A/B/C/A dedup, collision rollback, retention and
count/byte caps, encrypted reopen, protected-key persistence, corrupt/missing keys,
schema rejection, empty-file rejection, nonreused IDs, consent persistence, exclusive
writers, stale generations, shutdown and literal rendering of HTML-like text.
New behavior and the empty-file/ID regressions were observed failing before their fixes.
Native listener initialization and shutdown were tested while capture was inactive.

## Unverified and operational limits

The active native clipboard harness requires a private Windows window station. Its creation
was denied locally with error 5; it never fell back to the interactive clipboard. Active
Win32 handle parsing, native notifications and backpressure remain unverified locally.
Windows Debug and manual Full Verification CI explicitly enable this test and fail if
isolation is unavailable. No remote workflow has run; no CI timing or cache-hit claim is made.

Local compiler caching was disabled. Measured build files occupied approximately 838 MiB
for Debug, 340 MiB for Release, 98 MiB for staging and 1703 MiB for local build/quality tools.
These are file-size observations, not compiler-cache utilization measurements.

There is no paste/replay, search, pinning, image/HTML capture, recovery UI or single-instance
activation yet. Same-user malware, unmarked secrets, delayed rendering and physical secure
deletion remain limitations in the [threat model](../security/threat-model.md).
NSIS installer execution and macOS desktop integration remain unverified.

Next implement safe text replay with foreground-target validation and a clipboard provenance
marker, preserving original payloads and testing sensitive-format handling with synthetic
data in the isolated native harness before claiming native replay support.
