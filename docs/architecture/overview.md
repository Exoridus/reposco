# Architecture

src/app owns one Qt event loop and DesktopSession. The UI module owns QML loading, QWindowKit and tray actions. The palette is hidden by default; closing it hides it. QApplication does not quit when the last window closes. Tray Quit explicitly ends the event loop.

reposco_core is plain C++20 without Qt or native dependencies. Its bounded text admission policy is a testable starting point, not a complete security filter or Unicode normalization implementation. Future normalization, classification and history policies belong here.

reposco_storage owns SQLCipher connections/statements. EncryptedDatabase is synchronous, requires a key, validates an authenticated schema read and rejects unreadable files. Future use must be on one storage worker. The bootstrap app never opens a database on the GUI thread.

reposco_platform_windows owns user-scope DPAPI and thread-bound hotkeys. Its native filter receives WM_HOTKEY without a keyboard hook. No capture/paste backend exists. Introduce portable contracts when a concrete second backend needs them.

Dependencies flow down from app/UI to platform/core. Storage links pinned SQLCipher and source-built OpenSSL. QWindowKit private Qt ABI dependencies stay in its integration and UI adapter. Widgets is used for QApplication/QSystemTrayIcon; QWindowKit Widgets is disabled.
