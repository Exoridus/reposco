# Branding validation

Validated locally on 2026-10-11. This records the selected branding integration,
not completion of the v0.1.0 product or native clipboard end-to-end validation.

## Assets and integration

The exact Creative_01 1024px PNG and multiresolution ICO, original light
transparent wordmark and corrected primary 2048x768 banner are in
`assets/branding/`. These are raster assets with no bundled font or vector claim.
Their bytes match the supplied exports. Alternatives and source renders remain
in ignored `.workspace/`. The placeholder SVG was removed.

Qt resources provide the application/window and tray ICO and the palette
wordmark. The Windows executable embeds the same ICO through the existing CMake
build. README uses the primary banner by relative path. Inspection at 16, 32 and
64 px on light and dark surfaces found no clear alpha-edge problem requiring edits;
final small-size and tray appearance still need hands-on acceptance.

## Executed checks

- `./tools/dev.ps1 quick`: formatting, Debug build and 7/7 CTest suites passed.
- `./tools/dev.ps1 full`: repository/dependency/license/workflow checks, Debug
  build, QML lint, 7/7 suites, clang-tidy/cppcheck, Release build and deployed
  package startup all passed. Analyzer diagnostics covered by the existing non-user-code and documented
  NOLINT suppressions were suppressed; no analyzer failure was reported.
- Debug `reposco.exe --smoke`: exited 0 on interactive `WinSta0`. A visible
  Reposco window was observed with nonzero small and large Windows icon handles.
  An app-only PrintWindow screenshot showed the original wordmark, paused capture
  notice, opt-in control and empty history. Capture remained disabled with a
  temporary profile; no real clipboard data was collected.
- The desktop suite exposed/grabbed a Qt window, verified tray availability,
  hide/show and Quit signaling, and exercised capture controls with a synthetic
  monitor. This does not verify the tray artwork or a real hotkey press.
- The authenticated private GitHub repository was created and the original
  bootstrap commit `62a1ad85927fa17f8c74294f9cc41ee7686205b3` was pushed and
  independently matched with `git ls-remote`. No remote workflow run was listed;
  the current workflow runs on pushes to `main`, pull requests and dispatch.

Logs, surface review and the app screenshot are local under ignored `.workspace/`
(`branding-quick.log`, `branding-full.log`, `branding-surface-review.png`,
`reposco-branded-smoke.png`). The PrintWindow screenshot omits the Quit button
label; confirm its actual on-screen appearance manually.

## Hands-on inspection and remaining limits

The deployed development application is `build/package-stage/reposco.exe`.
Run it with `--smoke` for a temporary, capture-disabled window that quits
automatically, or `--show` for hands-on inspection. `./tools/dev.ps1 run` builds
Debug and starts the normal tray application. No installer was executed and no
package was published or signed.

In an appropriate interactive test session, using only synthetic text:

1. Confirm a fresh profile starts paused and presents the capture warning before
   explicitly selecting Start / resume text capture. There is currently an inline
   opt-in notice/control, not a separate consent dialog.
2. Copy A/B/C/A and confirm A moves to the top with only three entries. Check
   Pause/Resume and that resume does not import the current clipboard.
3. Press Win+Alt+V, inspect focus and hiding, and inspect the tray icon/menu.
   Paste/replay is not implemented; focus/paste end-to-end cannot pass yet.
4. On a disposable test profile, confirm encrypted history cannot be opened with
   no key or a wrong key. Automated synthetic security tests passed, but manual
   inspection of a normal profile was not performed.

The privileged isolated active native clipboard suite remained disabled; it was
not rerun. Native clipboard notifications, marker parsing and backpressure remain
unverified here. Portable Linux sanitizer tests and installer execution were not
rerun for this branding-only change. Other implementation/security limits remain
in [text-history validation](text-history-validation.md) and the
[threat model](../security/threat-model.md).
