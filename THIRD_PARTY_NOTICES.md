# Third-party notices

Exact recursive source revisions and license files are recorded in [the dependency manifest](third_party/deps.lock.json). Source checkouts are unmodified Git submodules.

| Component | Version | License | Distribution |
| --- | --- | --- | --- |
| Qt Core, Gui, Widgets, Qml, Quick, QuickControls2, Svg | 6.12.0 | LGPL-3.0-or-later or GPL-3.0-or-later; individual files may use BSD terms | Dynamic DLLs and deployed QML/plugins |
| QWindowKit | 1.5.0 | Apache-2.0 | Static Core/Quick |
| SQLCipher | 4.19.0 | BSD-3-Clause | Static library |
| SQLite upstream portions | See SQLCipher source VERSION | Public domain | Included in SQLCipher |
| OpenSSL | 3.5.9 | Apache-2.0 | Static crypto |
| qmsetup and syscmdline | Manifest revisions | MIT | Build-time only |
| Strawberry Perl | 5.40.5.1 | Perl Artistic/GPL; bundled tools have their own terms | Build-time only |

Apache-2.0 and BSD terms are compatible with GPL-3.0-or-later distribution; see the [Apache compatibility explanation](https://www.apache.org/licenses/GPL-compatibility). Retain upstream copyright, license and NOTICE files where supplied. This is an engineering license inventory, not a legal audit.

Distributors must supply complete corresponding source for the exact application and statically linked dependencies, including build scripts and modifications. Preserve Qt LGPL notices, replacement/relinking rights and exact Qt source availability. Obtain matching Qt source packages from the official archive before publishing and review every deployed module/plugin license. MSVC runtime redistribution follows Microsoft's terms.

Packaging includes upstream notices, the manifest and Qt SDK license texts. Reposco's selected Creative_01 icon, original wordmark and primary banner derive from user-supplied raster artwork and are distributed with this project's GPL-3.0-or-later assets. No replacement font binaries or native vector artwork are bundled. The README banner is a pixel-identical lossless WebP derivative; the original high-resolution PNG remains available.
