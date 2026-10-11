# Reposco

An offline, privacy-oriented clipboard manager for Windows, built with C++20 and Qt Quick.

**Status: 0.1.0-dev engineering foundation.** The executable provides a QWindowKit palette, SoftGlass fallback, tray, Win+Alt+V hotkey and clean shutdown. Clipboard capture and history are not implemented. Synthetic tests exercise SQLCipher encryption and Windows user-scope DPAPI.

```powershell
git clone --recurse-submodules https://github.com/Exoridus/reposco.git
cd reposco
./tools/dev.ps1 setup
./tools/dev.ps1 build
./tools/dev.ps1 test
./tools/dev.ps1 run
```

Requires Windows x64, MSVC 2022 or a compatible newer compiler, Ninja, CMake >=3.27 and Qt **6.12.0**. Existing Qt installations are reused; pass `-QtRoot` to override. Use the tray to show or quit. Closing the palette keeps the tray process alive.

Portable core builds without Qt: `cmake --preset portable`, `cmake --build --preset portable`, `ctest --preset portable`. Linux/macOS desktop integration is not supported yet.

[Build and test](docs/development/build-and-test.md) | [Code quality](docs/development/code-quality.md) | [Architecture](docs/architecture/overview.md) | [Privacy](PRIVACY.md) | [Security](SECURITY.md) | [Contributing](CONTRIBUTING.md)

GPL-3.0-or-later. See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md). No release has been published.
