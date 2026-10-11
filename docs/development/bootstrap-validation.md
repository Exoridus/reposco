# Bootstrap validation

Validated on 2026-10-11. This records an engineering integration proof, not a completed clipboard-history product or a security audit.

## Tested environment and dependencies

Windows 11 x64 used Ninja, CMake 4.4.2 and Visual Studio 2026 MSVC 19.51.36260. The installed Qt 6.12.0 msvc2022_64 SDK compiled and rendered the QWindowKit window successfully. The MSVC 2022 CI toolchain has not yet run.

| Dependency | Version | Pinned source revision |
| --- | --- | --- |
| QWindowKit | 1.5.0 | 35e88f3655720ed0537c7dd1dde243bf4a70c94c |
| qmsetup | Build dependency | 85c6c3c783be8af8d3f2fa492748a82da8ec9bad |
| syscmdline | Build dependency | 5a67673ff96acbfd894ea653fbaca872fded758a |
| SQLCipher | 4.19.0 | c4b275a47932888216bade83aff2bbc73df0ff85 |
| OpenSSL | 3.5.9 | 45e844fa2a14ec92d146bd8f5778ac130b6625fb |

OpenSSL was updated to the current 3.5 LTS patch release after reviewing upstream security advisories. The integration test checks the linked crypto provider version. Exact provenance, tool pins and license records live in third_party/deps.lock.json and THIRD_PARTY_NOTICES.md.

## Executed validation

| Command or check | Actual result |
| --- | --- |
| `./tools/dev.ps1 setup` | Passed; reused Qt, checked exact versions and required submodules |
| `./tools/dev.ps1 format` | Passed |
| `./tools/dev.ps1 build` and Release build | Passed with warnings as errors on owned targets |
| `./tools/dev.ps1 quick` | Passed |
| `./tools/dev.ps1 test` | Passed; four real CTest targets |
| `./tools/dev.ps1 full` | Passed; final CTest run 4/4, 2.97 seconds |
| Repository hygiene and dependency/license checks | Passed |
| clang-format, qmlformat, qmllint, cmake-format, cmake-lint | Passed |
| clang-tidy and cppcheck | Passed across ten owned C++ translation units |
| actionlint and zizmor | Passed; narrowly documented workflow exceptions |
| Staged Release executable, isolated runtime search path | Passed; rendered window and exited through the actual tray Quit action |
| Scoped clean command in a disposable fixture | Passed; generated outputs removed and neighboring data preserved |
| Unsupported MSVC sanitizer configuration | Rejected explicitly as expected |

Windows tests exercise DPAPI round-trip and rejection, SQLCipher identity/provider/integrity, encrypted file reopen, wrong and missing keys, independent plaintext SQLite rejection, real hotkey conflict and cleanup, window rendering, close-to-tray behavior and QML fallback properties. Only synthetic data is used.

Existing WSL Linux used GCC 15.2 and verified official CMake 3.31.8. Both commands below configured, built and executed one real portable-core test successfully:

```sh
cmake --preset portable -G "Unix Makefiles" -B build/portable-linux
cmake --build build/portable-linux
ctest --test-dir build/portable-linux --output-on-failure
cmake --preset portable-sanitizers -G "Unix Makefiles" -B build/portable-sanitizers-linux
cmake --build build/portable-sanitizers-linux
ctest --test-dir build/portable-sanitizers-linux --output-on-failure
```

The separate Linux directories avoid reusing a Windows CMake cache. ASan and UBSan were enabled and passed. No native macOS validation was performed.

## Timing, cache and artifacts

The final incremental full verification measured 1.45 seconds for Debug compilation and 15.26 seconds for Release compilation. An earlier Release rebuild after the OpenSSL update took 550.62 seconds. These are local observations, not remote CI performance claims.

An isolated MSVC portable-core sccache proof produced two cold misses followed by two warm hits, with no cache errors. The warm build took 0.502 seconds and its cache used approximately 115 KiB. This proves compiler caching for that target only.

Measured generated directories: tools 1703.05 MiB, Debug 527.45 MiB, Release 317.30 MiB, deployment stage 94.04 MiB. Other proof/build directories total approximately 13.19 MiB. Existing shared Qt and compiler installations are excluded. Build outputs and caches are ignored by Git.

The unpublished portable artifact is `build/Reposco-0.1.0-dev-windows-x64.zip`, 40,457,173 bytes, SHA256 `d1877d40ea69a45c95f9b34f569a03fbad0573e80a18bd55e6410c71e4224bc0`. It includes deployed Qt/MSVC runtime files and license texts. It is unsigned. Optional NSIS installer compilation, installation and uninstall remain unverified because NSIS was unavailable.

## CI and remaining limits

The workflow defines Fast Quality, Windows Debug, Windows Release, Portable Core, manually invoked Full Verification and Portable Sanitizers. Workflow validation passed locally. None of these jobs has run remotely: the GitHub repository has not been created or published. Remote duration and cache hit rates remain unmeasured.

No Git hooks were installed. ExoSnap was inspected read-only; ExoQuill was not modified. No release, tag, public repository or global Git configuration change was made.

The bootstrap does not implement clipboard capture, full Unicode normalization, persistent production key management or history. DPAPI and database encryption do not protect against a process already running as the same user. See the threat model for the security boundary.

The next product slice is bounded Windows STA text capture with exclusions checked before payload reads, core admission/normalization, a dedicated SQLCipher history worker using a randomly generated DPAPI-protected key, and a minimal QML history list. Establish the production key lifecycle before storing actual clipboard data.
