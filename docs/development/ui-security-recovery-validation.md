# UI and capture-security validation

This records the recovery milestone and its limits. Tests use synthetic text and
temporary encrypted profiles. Default/visual tests do not write the interactive
clipboard. A separate coordinated test mode exercises Windows input only after
current desktop authorization, with synthetic values and owned fixture windows.
No password-manager vault access is performed.

## Capture boundary

The previous implementation already screened sensitive Windows formats before
reading text, honored explicit local-history denial, preserved opt-in and pause
generations, and used SQLCipher with a DPAPI-protected random key. Encryption did
not prevent unmarked secrets from being captured. Exact OTP exclusion and native
application rules were missing.

The new default excludes complete ASCII six-digit values after trimming only
surrounding Unicode whitespace, before canonicalization, hashing or persistence.
Eight-digit exclusion is separate and disabled by default. Neither rule modifies
the Windows clipboard. Tests cover these cases:

| Synthetic text | Default result |
| --- | --- |
| `123456` | Excluded |
| ` 123456\r\n` | Excluded |
| Unicode whitespace surrounding `123456` | Excluded |
| `12345`, `1234567`, `code 123456` | Capturable |
| Full-width digits or an eight-digit value | Capturable |
| Six-digit exclusion disabled | Capturable |
| Eight-digit exclusion explicitly enabled | Excluded |

Core tests assert that explicit sensitive markers prevent payload reads even
when a local-history permission value is also present. The native adapter honors
`ExcludeClipboardContentFromMonitorProcessing`, `Clipboard Viewer Ignore`, and
bounded DWORD `CanIncludeInClipboardHistory`; malformed local-history permission
data fails closed. `CanUploadToCloudClipboard` does not suppress local history.
Actual active Win32 marker/handle tests remain gated behind the isolated window
station suite; local creation of that station was previously denied (error 5).

Native application rules match full executable paths, case-insensitively, before
reading text. When rules exist, unavailable clipboard-owner provenance fails
closed. Tests cover matching, path validation and policy persistence. Ordinary
browser clipboard ownership cannot identify an individual extension. There is
no general password detector and no default browser-wide exclusion.

Integration tests cover `A -> B -> C -> A` deduplication, excluded entries absent
from storage, persisted filter changes, stale queued captures during clear,
pause/no scan on resume, confirmed retention reduction and bounded preview/search.
Clear requires confirmation, pauses capture, removes stored text and comparison
hashes, invalidates preview/search replies and clears the visible model. The
database uses `secure_delete` and DELETE journaling, not WAL. This is logical
deletion, not a forensic erasure guarantee for SSDs, backups or process memory.

General user regex rules are deferred. A future implementation must use bounded
full-match rules and an engine with predictable execution cost, reject invalid
patterns, and filter before fingerprint generation. Arbitrary backtracking on
large clipboard payloads is outside this milestone.

## Palette and lifecycle

The former large onboarding/history placeholder is replaced by a compact
680-pixel palette: small approved branding, dominant search, bounded plain-text
preview, four visible virtualized slots and keyboard hints. Settings is a
separate scrollable window. Capture warnings, filters, application rules,
retention, confirmed deletion and shortcut choices live there.

Dark/light states use a solid, rounded glass-style fallback. Native desktop
backdrop blur is not validated or claimed. Appearance, reduced motion and
alternative hotkeys currently apply to the session. Search performs literal
matching with ASCII case folding; it is bounded to 256 characters. Preview is
bounded to 8192 original-text characters. Explicit replay retrieves the full original asynchronously. Enter and Shift+Enter paste Unicode text; Ctrl+Enter copies only. No richer original formats are claimed.

Open requests now show idempotently instead of toggling. Repeated hotkey dispatch
cannot immediately hide the palette. Search receives focus on open/activation;
focus loss does not automatically hide the window. Escape intentionally hides
the active palette. The coordinated OS hotkey test reproduced a process crash
with exception 0xc0000005 in GlobalHotkey::nativeEventFilter: Qt's thread-message
path supplies a null result pointer, which the handler dereferenced after opening.
A deterministic null-pointer regression failed before the guard and passes after it.
Hotkey activation waits briefly for shortcut release to avoid Alt-release focus
races; open requests remain idempotent.

The visual regression exposed a separate real defect: an Escape shortcut in the
hidden Settings window intercepted palette Escape. Both shortcuts now require
their owning window to be visible and active; the previously failing sequence
passes. Default Windows-style controls also rejected customization; the owned
controls now explicitly use Qt Quick Controls Basic. Generated QML cache C4702
warnings in Qt headers are suppressed only for affected generated sources;
owned C++ retains warnings as errors.

Desktop tests expose the real QQuickWindow, capture synthetic empty/populated/
search states in both themes, inspect Settings, exercise selection/search,
hide/reopen and search focus. They dispatch `WM_HOTKEY` directly to Reposco's
own handler, without injecting input into Windows. Lifecycle logs are tested
against a payload-free fixed-message whitelist. These tests do not prove
physical hotkey behavior, other applications' focus transitions, dual displays
or virtual desktop transitions.

## Hands-on checks

After building, `./build/windows-debug/reposco.exe --show` opens the real profile.
Use `--smoke --show --diagnostics` for a temporary, capture-disabled launch that
exits automatically. The development package is staged in `build/package-stage`;
it is neither installed nor published by packaging.

For visual regression screenshots, set `REPOSCO_VISUAL_OUTPUT` to an ignored
output directory and run the `integration.desktop` test. This renders synthetic
fixtures only. A changed theme or scale factor must be visually inspected, not
inferred from headless tests.

Perform these checks manually in a coordinated disposable interactive session:

1. Confirm first launch is paused and capture requires explicit Settings consent.
2. Copy synthetic A/B/C/A; confirm one A at the top, then C and B.
3. Copy synthetic `123456` and surrounding-whitespace variants; confirm they
   still paste normally from Windows but do not appear in Reposco. Confirm
   `12345`, `1234567` and `code 123456` do appear.
4. With throwaway Bitwarden entries only, inspect autofill/TOTP copy behavior.
   Browser-extension passwords without markers can still be captured. Pause
   before any real secret activity and explicitly clear any previously stored
   sensitive history; clearing Windows clipboard alone does not clear Reposco.
5. Test physical Win+Alt+V, rapid repetitions, tray open, Escape, reopening and
   search focus. Check 100%/150% scale, secondary monitors and virtual desktops.
6. Verify pause/resume does not import current clipboard. Confirm Clear history
   cancels safely or empties history while keeping the Windows clipboard intact.
7. On a disposable copy of a profile, confirm the encrypted database cannot be
   opened without its key. Never remove the real profile key to perform this test.

Remote CI status is checked separately. Bootstrap now triggers the existing
private workflow. The first actual run exposed an SDK install/cache path mismatch;
install-qt-action now installs under github.workspace to match the declared Qt path.
Run 38156878340 completed Fast Quality, Portable Core, Windows Debug and Windows
Release successfully. Full Verification and Portable Sanitizers were skipped on
that push; they require the manual full input.

## Local evidence (2026-10-11)

The final Debug CTest pass completed all seven registered suites, including the
new privacy and palette regressions. Repository, CMake/workflow/license checks,
QML formatting/lint, owned C++ clang-tidy and cppcheck were run. Initial gates
failed on CMake formatting/lint disagreement, a declaration parameter name,
cppcheck's buffer initialization analysis and reference-return suggestions.
Those findings were corrected and the affected checks rerun; analysis was not
repeated for unrelated sources after the targeted corrections.

Debug and Release builds used the requested six-job local default. A separate
policy probe returned six locally and the full 16 available processors under
both CI flags, including when a local override was supplied. CI was not run
locally; this proves the script branch, not remote runner performance.

The Debug executable ran on interactive WinSta0 with a temporary profile and
capture disabled. Its visible Reposco HWND and small/large Windows icon handles
were observed; its timed shutdown returned zero. The deployed Release executable
also passed the package smoke with SDK paths removed. The tray-availability API
passes in desktop tests, but physical tray clicking and shell icon appearance
have not been manually verified.

Real Qt window screenshots cover empty, populated and search states in both
themes, plus Settings. The synthetic keyboard/focus sequence passed again with
`QT_SCALE_FACTOR=1.5`; clipping and contrast were inspected in the rendered
images. Settings now bounds its initial size to available screen dimensions.
This is Qt scaling validation, not a physical per-monitor DPI transition test.
Physical Win+Alt+V, Bitwarden interaction, isolated native clipboard markers,
dual displays and virtual desktop transitions remain manual checks. Linux
sanitizers were not rerun for this milestone. No installer was executed and no
package was published.

## Native usability follow-up (2026-10-11)

Coordinated WinSta0 tests used synthetic text and temporary encrypted profiles.
Actual OS Win+Alt+V dispatch reproduced the crash before the result-pointer guard.
Subsequent open/search-focus, rapid repeated hotkey, Escape and reopen sequences
passed. One early post-fix run failed a foreground assertion; it was not counted
as success. These are coordinated SendInput tests, not manual physical-key proof
across every monitor, DPI or virtual desktop.

Exact `123456` and surrounding-whitespace variants stayed in the Windows clipboard,
were absent from history/search, and pasted into an owned native EDIT. Native
process exclusions also prevented capture. Replay marker publication preserved
Unicode text, did not emit another capture and did not add a history row.

The final coordinated replay fixture ran in a separate non-elevated process:
Ctrl+Enter copied without automatic paste and kept the palette open; Enter and
Shift+Enter restored full original text and pasted into the previously active
native EDIT. CRLF and trailing spaces were preserved. Earlier fixture attempts
failed on premature activation checks and on preexisting EDIT text; these were
corrected before accepting the final run. Bitwarden itself was not accessed.

Worker tests cover queued capture policy-generation changes, full originals longer
than the 8192-character preview, clear cancellation, unsupported writes and one
outstanding replay read after cancellation. A failing queue regression led to a
separate storage-pending flag. Delayed replay completion must not reclaim focus
from another window. Policy changes invalidate queued work; an already-started
storage transaction may still finish, as documented in Privacy.

Active isolated Windows tests remain enabled in CI; local CreateWindowStation
is denied with error 5. Coordinated interactive tests are explicit opt-in, not
an isolation fallback. Default desktop suites skip the two coordinated input
cases. Manual Bitwarden/browser-extension checks with throwaway credentials,
physical tray inspection, elevated/UIPI fault scenarios, multi-monitor and
virtual-desktop transitions remain unverified. General unmarked passwords are
still outside reliable detection. Glass/material refinement and richer formats
remain later work; the approved palette layout is retained.

The integrated follow-up passed dev.ps1 quick (7/7 local suites), then one
final dev.ps1 full: repository/workflow/license/format checks, Debug QML lint and
7/7 tests, all 21 owned clang-tidy/cppcheck sources, Release build and deployed
package smoke. Both local builds explicitly reported six jobs. Refreshed real
Qt screenshots in dark/light, empty/populated/search and Settings passed the
visual harness at 100% and 150%; populated frames were visually inspected.
The portable development ZIP is 36,304,771 bytes, SHA256
`1a3a06be45ffcdfb8c6f0cfdc95cec622456ec589d8efade79e111a95a9740e6`.
It is staged locally, not installed or published. Native replay is text-only;
a sent paste request is not a guarantee for every third-party application.

A separate Release CTest run also passed all seven suites. The repository has no
Release test preset, so it used ctest --test-dir build/windows-release.
