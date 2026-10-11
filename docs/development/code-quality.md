# Code quality

clang-format 19.1.7 uses LLVM style at 100 columns. QML uses Qt 6.12.0 qmlformat. CMake uses cmakelang 0.6.13. Owned targets use /W4 /WX /permissive- /sdl and enum-switch warnings, or -Wall -Wextra -Wpedantic -Werror. Vendor targets do not inherit these.

Use `dev.ps1 format`, `quick`, `full`. SDK-free checks use `tools/quality.ps1 -Mode Fast`; QML checks belong to Windows Qt. Full runs clang-tidy 22.1.0 on owned compile commands, cppcheck 2.21.0 with Qt/Windows models, cmake-format/cmake-lint, actionlint 1.7.12 and zizmor 1.30.1. Missing tools fail. CTest noTestsAction=error prevents empty green suites.

Curated tidy checks cover lifetime, slicing, initialization, performance, modernization, naming and indentation. Qt/Win32 ownership/cast checks are excluded because they cannot model parent-owned QObjects and opaque handles. Suppressions must be local and explain a durable reason.

The generated Main.qml cache alone disables MSVC C4702: Qt 6.12 generated QVariant/QJS casts trigger unreachable-code warnings in Qt headers with the tested newer MSVC. Project-owned C++ retains all configured warnings and /WX.

Two native interop exceptions are local: clang-tidy permits recovering a pointer from Win32's integer-sized window userdata, and cppcheck permits freeing synthetic clipboard memory after failed SetClipboardData. Its Windows model assumes unconditional ownership transfer; [Windows transfers ownership only on success](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setclipboarddata).

Scoped hygiene checks LF, ASCII punctuation, justified NOLINT, public-header namespace pollution and core native/storage/UI includes. Dependency checks validate exact revisions, clean trees and license files. Human license review remains required before distribution.

No mandatory hooks or silent skipped tools. A check never run is unverified. Run the full gate once for integration; subsequent checks target new findings.
