# UI and capture-security validation

This records the recovery milestone and its limits. Tests use synthetic text and
temporary encrypted profiles. No interactive clipboard write, password-manager
vault access or desktop-wide input injection is required by the visual harness.

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
bounded to 8192 original-text characters. Paste/replay remains unavailable.

Open requests now show idempotently instead of toggling. Repeated hotkey dispatch
cannot immediately hide the palette. Search receives focus on open/activation;
focus loss does not automatically hide the window. Escape intentionally hides
the active palette. The observed original intermittent flash was not reproduced
with a physical shortcut, so its historical cause remains unconfirmed.

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

Remote CI status must be checked separately. Local green tests do not establish
a CI result; the existing workflow does not automatically run on bootstrap pushes.

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
