# Reposco

![Reposco](assets/branding/reposco-banner-primary.png)

An offline, privacy-oriented clipboard manager for Windows, built with C++20 and Qt Quick.

**Status: 0.1.0-dev text-history slice.** The executable provides opt-in Unicode text capture, SQLCipher-encrypted local history, a random DPAPI-protected key, Smart dedup, Pause/Resume, a plain-text history list, QWindowKit palette, SoftGlass fallback, tray and Win+Alt+V hotkey. First launch is paused. Capture consent and pause persist; resuming never imports the existing clipboard. Paste, search, pinning and image/HTML capture are not implemented yet.

```powershell
git clone https://github.com/Exoridus/reposco.git
cd reposco
./tools/dev.ps1 setup
./tools/dev.ps1 build
./tools/dev.ps1 test
./tools/dev.ps1 run
```

Requires Windows x64, MSVC 2022 or a compatible newer compiler, Ninja, CMake >=3.27 and Qt **6.12.0**. Existing Qt installations are reused; pass `-QtRoot` to override. Use the tray to show or quit. Closing the palette keeps the tray process alive.

Select **Start / resume text capture** to opt in. Marked secrets are ignored before text reads; unmarked passwords cannot reliably be recognized. History is limited to 1000 items, 90 days and 1 GiB of logical payload; the palette shows the latest 100 bounded summaries. Storage and capture run on separate workers. Missing keys or unsupported/corrupt history lock capture without replacing original files.

Portable core builds without Qt: `cmake --preset portable`, `cmake --build --preset portable`, `ctest --preset portable`. Linux/macOS desktop integration is not supported yet.

[Build and test](docs/development/build-and-test.md) | [Code quality](docs/development/code-quality.md) | [Validation](docs/development/text-history-validation.md) | [Architecture](docs/architecture/overview.md) | [Privacy](PRIVACY.md) | [Security](SECURITY.md) | [Contributing](CONTRIBUTING.md)

GPL-3.0-or-later. See [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md). No release has been published.
