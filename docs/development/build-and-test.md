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
