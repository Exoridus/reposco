# Stage A visual approval: Quick Paste and History Workspace

Stage A was visually approved by the user and integrated on bootstrap at
`f5740fd1bd0ad402e8d7654b91e969db3e40e77c`. The normal-user
`dev.ps1 full` integration run passed all eight CTest suites, Debug/Release,
QML checks, owned C++ analysis and portable package smoke. The earlier sandbox
results below are retained as historical evidence. Release qualification is
still in progress; no release or tag has been published.

## Application surfaces

Quick Paste follows Direction A: a fixed 600 x 334 logical-pixel utility window,
search first, six visible slot positions, compact type icons and a quiet capture
indicator. Win+Alt+V opens it. Enter replays original formats, Shift+Enter requests
plain text, Ctrl+Enter copies; successful copying dismisses the picker. Escape
hides it. Expand opens History Workspace with the shared selection.

History Workspace follows Direction B: a fixed initial 820 x 520 logical-pixel
window, approved archive icon and serif wordmark, history at left, bounded rich
preview at right, and a restrained copy footer. Tray click and Open History open
this persistent surface. Enter copies, Shift+Enter copies plain text. Settings
is a page inside this window; Escape returns to History without closing the
workspace. Settings can remain open while another application has focus.
The compact page scrolls to storage controls below the first viewport.

Both windows share one engine, HistoryController, storage worker, monitor and
history model. The selected entry identity survives expansion and model moves.
There is no extra editor or view-mode preference. The existing selectedEntryId
is the future editor handoff point. Workspace never initiates automatic paste.
Quick Paste preserves the external target captured before opening and keeps the
existing validity, focus, clipboard-sequence and modifier-release checks.
Each window owns a separate QWindowKit agent. Quick has no title-bar hit region;
Workspace registers only its live title bar and interactive header controls.

Widths/heights do not change with selection, results or preview content. Opening
Quick restores its nominal size before bounding it to the available work area.
Workspace content reveal takes 120 ms, row movement 90 ms, preview swapping has
no animation. Quick keyboard selection is immediate. Reduced motion and the
Windows client-area animation preference disable transitions. No idle animation,
shader or desktop capture backdrop was added.

Capture stays paused until deliberate consent, unless previously enabled. The
existing policy acknowledgments, generation barriers, OTP/application exclusions,
storage schema and original/plain replay contracts remain in place. Exclusion
does not modify the OS clipboard. Appearance, motion and hotkey overrides remain
session-scoped; System is the appearance default.

## Actual application screenshots

These are production Main.qml, WorkspaceWindow.qml and shared components rendered
by Qt Quick on Windows with synthetic summaries and the existing offline preview
renderer. They are not generated mockups. This rendering fixture has no active
storage/capture backend; Settings therefore shows capture unavailable and disables
capture/storage changes. It does not prove a successful encrypted-history workflow.
Markdown bitmaps now use a bounded 420-pixel layout to keep text legible in the
workspace column, preserving offline resource denial and the 2048-pixel height cap.

Samples use actual Qt device pixel ratios 1.0 and 1.5, not resized bitmaps.
This is visual acceptance evidence, not comprehensive DPI qualification.
The primary samples below are QQuickWindow scene readbacks.

| View | 100% | 150% |
| --- | --- | --- |
| Quick Paste, Dark | [PNG](../images/stage-a/100/quick-dark.png) | [PNG](../images/stage-a/150/quick-dark.png) |
| Quick Paste, Light | [PNG](../images/stage-a/100/quick-light.png) | [PNG](../images/stage-a/150/quick-light.png) |
| Workspace, Dark | [PNG](../images/stage-a/100/workspace-dark.png) | [PNG](../images/stage-a/150/workspace-dark.png) |
| Workspace, Light | [PNG](../images/stage-a/100/workspace-light.png) | [PNG](../images/stage-a/150/workspace-light.png) |
| Settings, Dark | [PNG](../images/stage-a/100/settings-dark.png) | [PNG](../images/stage-a/150/settings-dark.png) |
| Settings, Light | [PNG](../images/stage-a/100/settings-light.png) | [PNG](../images/stage-a/150/settings-light.png) |
| Markdown preview | [PNG](../images/stage-a/100/markdown-dark.png) | [PNG](../images/stage-a/150/markdown-dark.png) |
| Image preview | [PNG](../images/stage-a/100/image-dark.png) | [PNG](../images/stage-a/150/image-dark.png) |
| Table preview | [PNG](../images/stage-a/100/table-dark.png) | [PNG](../images/stage-a/150/table-dark.png) |
| Quick opaque fallback, Dark | [PNG](../images/stage-a/100/quick-fallback-dark.png) | [PNG](../images/stage-a/150/quick-fallback-dark.png) |
| Quick opaque fallback, Light | [PNG](../images/stage-a/100/quick-fallback-light.png) | [PNG](../images/stage-a/150/quick-fallback-light.png) |
| Workspace opaque fallback, Dark | [PNG](../images/stage-a/100/workspace-fallback-dark.png) | [PNG](../images/stage-a/150/workspace-fallback-dark.png) |
| Workspace opaque fallback, Light | [PNG](../images/stage-a/100/workspace-fallback-light.png) | [PNG](../images/stage-a/150/workspace-fallback-light.png) |

Native PrintWindow readbacks are also available for
[Quick Dark](../images/stage-a/100/quick-dark-native.png),
[Quick Light](../images/stage-a/100/quick-light-native.png),
[Workspace Dark](../images/stage-a/100/workspace-dark-native.png) and
[Workspace Light](../images/stage-a/100/workspace-light-native.png), with matching
files in the 150 directory. Rendering metadata is recorded at
[100%](../images/stage-a/100/rendering.txt) and [150%](../images/stage-a/150/rendering.txt).

## Native material and visual limits

The pinned QWindowKit 1.5.0 Windows adapter exposes `acrylic-material` for Quick
and `mica` for Workspace. On this Windows host, DwmGetWindowAttribute confirmed
DWM backdrop types 3 (TRANSIENTWINDOW) and 2 (MAINWINDOW), respectively.
[Microsoft documents these Windows 11 materials as Desktop Acrylic and Mica](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type).
The application checks the actual DWM attribute because the pinned adapter does
not propagate every DWM HRESULT. Shared QML tokens add a neutral/warm flat tint,
a thin contour and restrained selection cues above the native layer.

Qt scene and PrintWindow captures do not prove visible desktop blur. Foreground
screen crops were unavailable under the sandbox identity. Normal interactive
Windows inspection remains necessary for material acceptance; these tinted
surfaces are not evidence of a visibly working Acrylic backdrop. Native material
can also vary when inactive. Disabled transparency, unavailable DWM material and
High Contrast use the fallback; High Contrast uses the Windows palette.
Thin Acrylic/Windows Composition was not added, nor were dependencies upgraded.

## Historical sandbox validation

- Debug and Release builds passed with six local jobs.
- QML lint passed without warnings; formatting and git diff checks passed.
- Scoped clang-tidy passed for DesktopSession, HistoryController, HistoryModel,
  ContentPreview and DesktopTests. MSVC C4702 suppression remains restricted to
  observed Qt-generated QML cache files; owned sources keep warnings as errors.
- Eight focused desktop checks passed: separate windows/routing/Escape, settings
  focus persistence, tray reopening, 1000-row rapid navigation, model identity,
  native hotkey dispatch and the absent-result-pointer regression.
- Four QML checks passed for appearance/utility accent contrast, material fallback,
  reduced motion, rich preview/source switching and surface properties.
- Three targeted preview checks passed for offline resource denial, bounded
  Markdown and preserved image pixels. The 420-pixel Markdown test failed at 580
  before the renderer change, then passed.
- Rendering fixture passed at both scales. The initial two-window lifecycle test
  failed before implementation and passed afterward. The diagnostics test now
  explicitly enables logging during its own capture rather than relying on an
  earlier test's logging state.
- Both `dev.ps1 quick` and the single final `dev.ps1 full` attempt stopped at
  CTest: 4 of 8 suites passed; security/history failed and desktop/history-flow
  timed out. The untouched `dpapiRoundTrip` independently fails at
  `wrapped.has_value()` under CodexSandboxOffline. Storage-backed selection,
  consent, search and replay validation is not claimed. DPAPI was not weakened.
- The full command's later analysis/Release/package steps were not reached.
  Scoped analysis, Release and portable package checks ran separately.
- Portable package smoke launch passed. ZIP SHA256: `e10c5383f00b5d1c34458f92c98f86ac04f279ef9066679ed19109e8ff4dc453`.
- Coordinated native keyboard injection, real third-party paste, mixed DPI and
  Stage B compatibility/security/release qualification were not executed.

## Launch and handoff

Run from a normal interactive Windows user session:

```powershell
./build/package-stage/reposco.exe --quick # Quick Paste
./build/package-stage/reposco.exe --show  # History Workspace
```

The application remains one process. If it is already running, use Win+Alt+V
for Quick Paste and the tray or Expand for Workspace.

Reproduce the rendering fixture without interacting with the OS clipboard:

```powershell
$env:PATH = "C:/Qt/6.12.0/msvc2022_64/bin;$env:PATH"
$env:QT_PLUGIN_PATH = "C:/Qt/6.12.0/msvc2022_64/plugins"
$env:QML_IMPORT_PATH = "C:/Qt/6.12.0/msvc2022_64/qml"
$env:QT_SCALE_FACTOR = "1" # Use 1.5 for the second set.
$env:REPOSCO_VISUAL_OUTPUT = "$PWD/.workspace/visual-review"
./build/windows-debug/desktop_tests.exe syntheticPresentationSamples -o -,txt
```

During the original sandbox validation, local HEAD and the connector-verified
remote bootstrap SHA both remained
`179240d6fc9c05585be344d5053094bad7e1d9eb`. No Stage A commit or push succeeded. The filesystem denies Git index
writes, Git transport is offline, and the connector rejected a blob write because
approval is required while this session's approval policy is never. No protected
state or remote history was overridden.

Source, screenshots and the portable ZIP are available locally. A complete binary
patch is saved at `.workspace/stage-a-two-surfaces/stage-a.patch` for recovery on
the recorded base. Review and commit the existing working tree from an authorized
normal shell before pushing bootstrap. No tag or release was published.

The user subsequently approved Stage A visually. Release qualification is a
separate decision and does not authorize Stage B.
