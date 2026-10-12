# Build and test

Use Windows 11 x64, Ninja, CMake >=3.27, MSVC x64 tools and Qt 6.12.0 msvc2022_64. dev.ps1 imports MSVC into its own process. Qt discovery uses REPOSCO_QT_ROOT, QT_ROOT_DIR or standard locations. Override with `-QtRoot <path>`. No global SDK/Git configuration changes.

```powershell
./tools/dev.ps1 setup
./tools/dev.ps1 build
./tools/dev.ps1 test -TestFilter integration.security
./tools/dev.ps1 run
./tools/dev.ps1 format
./tools/dev.ps1 quick
./tools/dev.ps1 full
./tools/dev.ps1 clean
```

setup checks out pinned sources and required recursive QWindowKit modules and provisions hash-verified build-only Perl in build/tools. Downloads happen here, not during configure or official release builds. Unused OpenSSL upstream test repositories are not fetched. CMake builds OpenSSL/SQLCipher from local source. clean removes only this repository's build outputs including Perl; rerun setup afterward. Shared SDKs, caches and repositories are preserved.

build is incremental Debug. test builds then runs CTest, optionally filtered. quick checks formatting and builds/tests Debug. full runs repository/workflow/license checks, Debug/QML/tests, owned C++ analysis, Release and staged package launch. Missing tools/empty suites fail. Install tools from [code quality](code-quality.md). `-UseSccache` enables optional caching and reports stats without changing the global cache directory.

Debug/Release outputs are isolated in build/windows-debug and build/windows-release. Ninja exports compile_commands.json; Debug uses /Z7. Crypto libraries are static and never selected from PATH. Packaging creates a development portable ZIP using Qt deployment. Nothing is published or signed.

After building Release, `./tools/package.ps1 -Installer` also compiles the per-user NSIS installer when NSIS 3 is installed. Installer compilation fails explicitly if NSIS is missing. Installer execution and uninstall behavior require a separate disposable Windows environment before release.

Linux/macOS use the Qt-free `portable` preset. `portable-sanitizers` adds GCC/Clang ASan/UBSan. These do not claim native desktop support. Run `ctest --preset portable` after building.

CI lanes are Fast Quality, Windows Debug, Windows Release and Portable Core. Manual Full Verification adds analysis, sanitizers and package checks. Actual remote status/timings require an authorized push and Actions run.

Text-history tests use temporary profiles and synthetic monitor adapters. Native listener lifecycle is tested while inactive. Active Win32 clipboard tests create a private window station before Qt initialization; they never fall back to the interactive clipboard. Windows Debug and manual Full Verification CI enable `REPOSCO_ISOLATED_CLIPBOARD_TESTS=ON`. Creating a named window station requires suitable Windows privileges; it was denied locally (error 5), so active native handle validation remains unverified here. To run it in an appropriate disposable Windows environment:

```powershell
cmake --preset windows-debug -DREPOSCO_ISOLATED_CLIPBOARD_TESTS=ON
./tools/dev.ps1 test -TestFilter integration.clipboard_isolated
```

An enabled isolation test fails when privileges or isolation are missing. Leave the option OFF on an ordinary local account. `reposco.exe --smoke` always uses temporary storage and disabled capture, regardless of the real profile's saved preference.

Local dev.ps1 builds default to six parallel jobs, capped to the available
logical processors with at least one job. CI (`CI=true` or `GITHUB_ACTIONS=true`) uses the full
available processor count, regardless of `-BuildJobs`. `-BuildJobs <count>`
overrides the local default; `./tools/dev.ps1 build -BuildJobs 1` minimizes local load. CTest remains
sequential by default. Prefer a focused `-TestFilter` while iterating; reserve
full analysis and packaging for integrated completion. Direct CMake invocations
retain their normal parallelism unless `--parallel <count>` is supplied.

## Coordinated interactive diagnostics

Only with current user authorization and a free Windows desktop, set
`REPOSCO_COORDINATED_DESKTOP=1` for the test process. Run desktop_tests with
`coordinatedNativeHotkeyKeepsPaletteOpen` or
`coordinatedTextReplayIntoOwnedNativeFixture`. The latter creates its own native
EDIT in a separate temporary test process. They send Windows key events and must
never run concurrently with another desktop/input test.

For native capture/OTP/self-write checks, native_capture_tests accepts
`--coordinated-clipboard exactCodesStayInPrivateClipboardAndOutOfHistory` only
with that environment flag. This deliberately overwrites the interactive
clipboard with synthetic values; it never saves/readbacks the previous clipboard.
The flag is not set by dev.ps1 or CI. Isolated tests never fall back to this mode.

Use `build/windows-debug/reposco.exe --quick` for Quick Paste and `--show` for History Workspace. The tray opens Workspace; Win+Alt+V opens Quick Paste. [Stage A visual review](stage-a-visual-review.md) records actual two-scale screenshots, material limitations and sandbox-blocked checks.
