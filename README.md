![Reposco](assets/branding/reposco-banner-readme.webp)

*reposcō · Latin, verb*
“I demand back; I reclaim.”

Reposco is an offline clipboard manager for Windows, built with C++20 and Qt Quick.
It keeps a searchable, encrypted local history and brings selected content back to
your application through Quick Paste, with a persistent History Workspace for
browsing and rich previews.

**Development preview.** No stable release or independent security audit has been
published. The current implementation supports bounded text and rich-content
formats; it does not preserve every Windows clipboard format.

## What works today

- Explicit opt-in capture, persistent pause/resume and smart deduplication.
- SQLCipher-encrypted history with a random key protected by Windows DPAPI.
- Unicode text, supported HTML/RTF and PNG/DIB images, with original-format replay.
- Search, bounded offline Markdown/image/table previews and a plain-text fallback.
- Configurable native application exclusions, exact-code filters and retention.
- A global Quick Paste shortcut, a persistent workspace with Settings and a tray menu.

![Reposco History Workspace with synthetic history and text preview](docs/images/stage-a/150/workspace-dark.png)

Actual Qt Quick window at 150% application scaling, populated with synthetic test
data. This demonstrates the interface, not a password-manager compatibility test.

## Install and run

There is no official download or installer release yet. Build from source using
the instructions below, then launch History Workspace:

```powershell
./build/windows-debug/reposco.exe --show
```

Open **Settings** and choose **Resume...** to opt in. First launch starts paused.
Closing either window leaves the tray process running. The tray opens Workspace;
Win+Alt+V opens Quick Paste. Resume does not import the current clipboard.

For a locally staged portable build, run `./build/package-stage/reposco.exe --show`
after packaging. Use `--quick` to open Quick Paste directly. `--smoke --show` uses
temporary storage with capture disabled.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| Win+Alt+V | Open Quick Paste, when registration succeeds |
| Up / Down | Select a history entry |
| Enter | Quick Paste: original-format paste; Workspace: copy |
| Shift+Enter | Quick Paste: plain-text paste; Workspace: plain-text copy |
| Ctrl+Enter | Quick Paste: copy supported original formats and dismiss |
| Escape | Hide Quick Paste; return from Settings to History in Workspace |

Type to search. Expand opens Workspace with the selected entry. Workspace stays
open when copying and never automatically pastes into another application.
Image-only entries have no plain-text replay. If the Quick Paste target loses
focus, is elevated or unavailable, or the clipboard is busy, automatic paste can
be cancelled; copy and manual paste remain the fallback. Dispatching paste input
does not guarantee that every destination application accepts it.

## Privacy and local storage

Reposco has no telemetry, account, sync or automatic updater. Previews do not load
web, local-file, UNC or embedded markup resources; copied URLs are never fetched.

Recognized Windows exclusion markers are checked before payload reads. After
trimming surrounding whitespace, standalone six-digit ASCII codes are excluded
from persistent history by default, before hashing or indexing. This can also skip
legitimate numbers; the filter is switchable. Eight-digit exclusion is optional.
Capture filters leave the Windows clipboard unchanged.

**Unmarked secrets can still be captured.** Browser clipboard writes cannot
reliably be attributed to a particular password-manager extension. Native
application exclusions use full executable paths. Pause capture before copying
unmarked sensitive content. Encryption at rest is not a guarantee of safe capture.

`history.db`, the DPAPI-protected `key.dat` and non-content `settings.ini` live in
the Windows per-user application data directory. History is capped at 1000 items
and 1 GiB of logical payload, with selectable 7/30/90-day retention. Physical
database size can remain larger. Confirmed **Clear history** pauses capture and
deletes entries logically; it does not promise forensic SSD erasure. Changing
filters does not retroactively remove existing entries. Losing the key can make
history unrecoverable.

See [Privacy](PRIVACY.md) and the [threat model](docs/security/threat-model.md).

## Development status

Windows x64 is the supported desktop target. Linux/macOS currently exercise only
the portable core. Rich formats use conservative size and structure limits;
arbitrary formats, pinning, sync and recovery/export are unavailable. QWindowKit
requests Acrylic for Quick Paste and Mica for Workspace, with a solid fallback.
Visible desktop blur still needs interactive validation; sandbox screenshots
prove the Qt presentation and configured DWM types. Stage A has visual approval;
release qualification has prepared local packages; outstanding compatibility and desktop
checks are recorded in the [qualification report](docs/development/release-candidate-validation.md).
Password-manager compatibility, third-party rich paste destinations and broader
multi-monitor/DPI behavior still need hands-on validation.

The latest capture/replay scope and actual checks are documented in
[rich-content validation](docs/development/rich-content-validation.md),
[UI/security validation](docs/development/ui-security-recovery-validation.md) and
[Stage A visual review](docs/development/stage-a-visual-review.md).

## Build from source

Requires Windows 11 x64, MSVC x64 tools, Ninja, CMake >=3.27 and Qt **6.12.0**
(`msvc2022_64`). Install the Qt SDK separately; an existing installation is reused.
Pass `-QtRoot <SDK path>` if discovery cannot find it.

```powershell
git clone --branch bootstrap https://github.com/Exoridus/reposco.git
cd reposco
./tools/dev.ps1 setup
./tools/dev.ps1 build
./tools/dev.ps1 test
./build/windows-debug/reposco.exe --show
```

Local builds default to **six jobs**; `-BuildJobs` overrides the local limit.
CI uses all available logical processors. Debug and Release outputs and caches
remain separate. To stage a development portable package:

```powershell
./tools/dev.ps1 build -Configuration Release
./tools/package.ps1
```

The Qt-free core uses `cmake --preset portable`, `cmake --build --preset portable`
and `ctest --preset portable`. See [build and test](docs/development/build-and-test.md)
for setup, focused tests and optional sccache support.

## Contributing and security

Bug reports and focused contributions are welcome. Start with
[Contributing](CONTRIBUTING.md), [architecture](docs/architecture/overview.md) and
[code quality](docs/development/code-quality.md). Use synthetic examples; never
attach clipboard databases, keys, credentials or unsanitized crash dumps.

Report vulnerabilities privately as described in [Security](SECURITY.md), rather
than in public issues.

## License and acknowledgments

Reposco is licensed under [GPL-3.0-or-later](LICENSE). It uses Qt, QWindowKit,
SQLCipher, SQLite and OpenSSL; exact revisions and redistribution notices are in
[third-party notices](THIRD_PARTY_NOTICES.md) and the
[dependency manifest](third_party/deps.lock.json).

The supplied Reposco icon, original wordmark and banner are raster artwork. No
replacement fonts or generated vector assets are bundled.
