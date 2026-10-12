# Windows release-candidate qualification

Qualification date: 2026-10-12. Stage A was accepted and integrated. This report
does not authorize publication or a v0.1.0 tag. Packages retain `0.1.0-dev`.

## Evidence and result

The integrated baseline is `434af048f7ffe72bcda292bdf37de60aa1287647`.
Its [push verification](https://github.com/Exoridus/reposco/actions/runs/38190969550)
and [manual full verification](https://github.com/Exoridus/reposco/actions/runs/38190970506)
passed, including Windows Debug/Release, QML, static analysis, portable core,
Linux/macOS portable sanitizers and isolated native clipboard tests. Portable
sanitizers do not qualify a macOS/Linux desktop application.

Local normal-user full verification passed eight CTest suites. Earlier DPAPI
failures under CodexSandboxOffline were environmental: the same tests passed
under the normal user without reducing encryption or key protection. Local
private-window-station creation returned Windows error 5; that isolated lane
was unavailable locally and passed on CI. Two stale test expectations were
corrected: copy status and incremental history-change notification.

The final qualification change corrects the installer's quoted uninstall command,
promotes NSIS compiler warnings to errors, records NSIS provenance and licenses,
and extends the existing synthetic replay test with German/Japanese text and
an observed hotkey timing. Clipboard production code and the approved UI were
not changed. Exact final source/artifact hashes and final verification logs live
in the generated local evidence package, outside source control.

## Windows interoperability

| Destination / operation | Result | Scope |
| --- | --- | --- |
| Owned native Windows EDIT, original/plain/copy-only | Passed actual target content checks | Unicode German/Japanese, CRLF and repeated spaces; original/plain replay plus copy-only assertions |
| Paint.NET 5.1.12, paste into new image | Passed actual destination observation | Synthetic PNG became a 3x2 image |
| Native rich clipboard publication/capture | Passed | CF_HTML, RTF and PNG; this proves publication/capture, not Office target fidelity |
| Firefox 157.0, fresh offline profiles | Unavailable / unconfirmed | First-run dialog and focus blocked reliable paste observation; no fidelity conclusion |
| Word/Excel/LibreOffice and browser copy source | Not executed | No qualified synthetic application session established |
| Bitwarden disposable credentials/TOTP | Not executed | No disposable profile established |

There is no universal fidelity claim for proprietary Office clipboard formats.
Browser-extension writes cannot always be attributed to the extension.
Unmarked passwords may be captured. Secret markers, OTP and executable filters
are boundaries with documented limitations, not password-entropy detection.

## Privacy and storage

The existing automated suites cover initial opt-in, pause/resume, secret markers,
six/eight-digit OTP policy, executable exclusions, generation synchronization,
self-write exclusion, capture shutdown, retention, clear/restart, encrypted
storage, key errors and migration. Coordinated native OTP/executable/self-write
tests also passed using temporary synthetic profiles. Existing security checks
were preserved. See [the threat model](../security/threat-model.md) for deletion,
DPAPI identity and recovery limitations. This run is not an exhaustive power-loss
or disk-fault campaign and does not establish forensic physical erasure.

## Desktop and visuals

Real global-hotkey activation, keyboard focus, persistent Workspace, shared state
and native target replay passed coordinated tests. Two monitors were inventoried:
2560x1440 and 1920x1080. Actual Qt presentation was captured at 100%, 150% and
200%; monitor inventory and scale fixtures do not establish mixed-DPI movement
or virtual-desktop behavior. High Contrast received automated presentation
coverage, not a complete real OS accessibility qualification.

[Stage A visual evidence](stage-a-visual-review.md) contains both approved
surfaces, dark/light, Settings, Markdown, tables and images. Additional 200%
and normal-desktop captures are in `.workspace/release-candidate/`.
Acrylic-enabled and fallback crops differ in tint, but recognizable background
detail was insufficient to prove visible blur. Successful DWM attributes are
not presented as proof of a visible material effect. No capture-based effect
or new rendering framework was added.

## Measured performance

All measurements use synthetic profiles. Values are observations, not targets.
Host: AMD Ryzen 7 3700X (8 cores, 16 logical processors), approximately 64 GiB
installed memory and NVIDIA GeForce RTX 5070 Ti. WMI's adapter-memory field is
not used as a reliable VRAM measurement. Approximately 101 GiB disk space was
free after qualification artifacts and the shared Qt source download.

| Measurement | Observed result | Limitation |
| --- | --- | --- |
| Instrumented application composition, first frame | 723 / 805 / 724 ms | Main-entry timing, fresh profiles; OS caches not flushed; not packaged-process cold launch |
| Same composition, idle CPU over 3 seconds | 0 / 15.625 / 15.625 ms | Coarse Windows process-time quantization; zero is not proof of zero usage |
| Same composition, working set | 92-99 MiB | Three samples after startup, not a long-running leak test |
| Real hotkey observed activation/frame | 341 / 67 / 63 ms | Includes QtTest polling; upper-bound observation, not precise presentation timestamp |
| SQLCipher search, 1,000 records, normal Windows run | Hit median 1.8264 ms; miss 1.5679 ms | 100 samples; backend only, excludes GUI scheduling |
| Bounded Markdown/image preview | Medians 209.9951 / 71.1044 ms | 32,227 characters / 2048x2048 pixels; ten samples, backend only |
| Create encrypted store and insert 1,000 records | 5,018.6188 ms | Known synthetic key, excludes DPAPI acquisition |
| Encrypted reopen | Median 222.4213 ms | Five samples |

An independent offscreen backend sample measured 249,856 bytes after 1,000
records. Retention kept 1,000 after inserting record 1,001. After logical removal
the database remained 253,952 bytes and reopened empty. Backend/platform samples
have different load and rendering contexts and are not regression comparisons.
End-to-end search latency and Acrylic/fallback frame-cost profiling remain open.

## Packages and licensing

Artifacts are `build/Reposco-0.1.0-dev-windows-x64.zip` and
`build/Reposco-0.1.0-dev-setup.exe`. Portable startup/shutdown smoke passed.
A fresh disposable Windows Sandbox passed per-user installation, correctly
quoted registry uninstall command, installed Quick Paste and Workspace smoke,
same-version reinstall, uninstall cleanup and preservation of a separate
synthetic profile sentinel. The developer's profile was not used.

NSIS 3.13 compiler ZIP provenance is pinned in the dependency manifest; its
SHA256 is `ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8`.
The installer explicitly uses zlib and includes the vendor license inventory.
Qt's matching official 6.12.0 source archive is cached under
`build/source-archives/`; SHA256
`98ff4f44bac6ec3e1e62ee2a4316ae0e3d15badb015d753cc0268a20db52f165`.
The final source bundle includes pinned application and recursive manifest
dependencies. Preserve notices, Qt replacement/relinking rights, corresponding
source and applicable Microsoft runtime terms before binary distribution.
This is an engineering inventory, not a completed legal audit of every deployed
plugin/runtime. See [third-party notices](../../THIRD_PARTY_NOTICES.md).

Repeat builds with `./tools/dev.ps1 full`; then place verified NSIS on PATH and
run `./tools/package.ps1 -QtRoot C:/Qt/6.12.0/msvc2022_64 -Installer`.
Local default remains six jobs; CI keeps available processor parallelism.
Launch `build/windows-release/reposco.exe --quick` or `--show`; `--smoke`
always isolates storage and disables capture.

## Acceptance and remaining risks

**NO-GO for declaring a fully qualified public v0.1.0 candidate.** The source
and prepared packages are available for further acceptance; no release/tag was
published. Green CI is not visual or interoperability approval.

1. High: Office/browser rich target fidelity and disposable Bitwarden tests
   remain unqualified; retain explicit compatibility/security limitations.
2. High: final binary-distribution license/source review remains required.
3. Medium: mixed-DPI movement, virtual desktops, real High Contrast and disabled
   transparency require desktop acceptance; visible Acrylic remains unproven.
4. Medium: two transient early coordinated replay failures were not reproduced
   over subsequent repeat runs; their cause was not established.
5. Low: end-to-end 1,000-entry UI timing and longer idle/render sampling remain
   unmeasured. Backend timings cannot substitute for them.

Before publication: review the final exact-SHA CI and artifact manifest, complete
the outstanding manual compatibility/desktop checks, review legal distribution
requirements and obtain explicit release authorization. No speech, formatter,
dictation or model-management features are included in this candidate.
