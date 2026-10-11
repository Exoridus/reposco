#include "platform/windows/KeyVault.h"
#include "platform/windows/NativeClipboardMonitor.h"
#include "ui/HistoryController.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QtTest>
#include <Windows.h>
#include <cstring>
#include <iostream>
#include <sddl.h>

namespace {
bool isolatedClipboard = false;
bool coordinatedClipboard = false;
class IsolatedStation final {
  public:
    IsolatedStation()
        : original_station_(GetProcessWindowStation()),
          original_desktop_(GetThreadDesktop(GetCurrentThreadId())) {
    }
    ~IsolatedStation() {
        if (switched_) {
            SetProcessWindowStation(original_station_);
            SetThreadDesktop(original_desktop_);
        }
        if (desktop_) {
            CloseDesktop(desktop_);
        }
        if (station_) {
            CloseWindowStation(station_);
        }
    }
    IsolatedStation(const IsolatedStation&) = delete;
    IsolatedStation& operator=(const IsolatedStation&) = delete;
    [[nodiscard]] bool initialize() {
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:P(A;;GA;;;OW)(A;;GA;;;SY)", SDDL_REVISION_1, &descriptor, nullptr)) {
            return false;
        }
        const std::unique_ptr<void, decltype(&LocalFree)> allocation(descriptor, LocalFree);
        SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
        const auto name = L"ReposcoSynthetic-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                          std::to_wstring(GetTickCount64());
        station_ =
            CreateWindowStationW(name.c_str(), CWF_CREATE_ONLY, WINSTA_ALL_ACCESS, &attributes);
        if (!station_ || !SetProcessWindowStation(station_)) {
            return false;
        }
        switched_ = true;
        desktop_ =
            CreateDesktopW(L"ReposcoSynthetic", nullptr, nullptr, 0, GENERIC_ALL, &attributes);
        return desktop_ && SetThreadDesktop(desktop_);
    }

  private:
    HWINSTA original_station_;
    HDESK original_desktop_;
    HWINSTA station_ = nullptr;
    HDESK desktop_ = nullptr;
    bool switched_ = false;
};

bool setPayload(UINT format, const QByteArray& bytes) {
    const auto allocation = GlobalAlloc(GMEM_MOVEABLE, static_cast<SIZE_T>(bytes.size()));
    if (!allocation) {
        return false;
    }
    auto* destination = GlobalLock(allocation);
    if (!destination) {
        GlobalFree(allocation);
        return false;
    }
    std::memcpy(destination, bytes.constData(), static_cast<std::size_t>(bytes.size()));
    GlobalUnlock(allocation);
    if (!SetClipboardData(format, allocation)) {
        // Ownership transfers only on success; cppcheck assumes every call frees it.

        // cppcheck-suppress doubleFree
        GlobalFree(allocation);
        return false;
    }
    return true;
}

bool writeSynthetic(HWND owner, const QString& text, const wchar_t* marker = nullptr,
                    const QByteArray& markerBytes = {}) {
    wchar_t name[256]{};
    DWORD required = 0;
    // WinSta0 writes require the separate, explicitly coordinated interactive mode.
    if (!GetUserObjectInformationW(GetProcessWindowStation(), UOI_NAME, name, sizeof(name),
                                   &required) ||
        (_wcsicmp(name, L"WinSta0") == 0 && !coordinatedClipboard) || !OpenClipboard(owner)) {
        return false;
    }
    bool success = EmptyClipboard() != FALSE;
    if (success && marker) {
        const auto format = RegisterClipboardFormatW(marker);
        success = format != 0 && setPayload(format, markerBytes);
    }
    QByteArray bytes(static_cast<qsizetype>((text.size() + 1) * sizeof(char16_t)), '\0');
    std::memcpy(bytes.data(), text.utf16(),
                static_cast<std::size_t>(text.size()) * sizeof(char16_t));
    success = success && setPayload(CF_UNICODETEXT, bytes);
    CloseClipboard();
    return success;
}
QString readSynthetic(HWND owner) {
    wchar_t name[256]{};
    DWORD required = 0;
    if (!GetUserObjectInformationW(GetProcessWindowStation(), UOI_NAME, name, sizeof(name),
                                   &required) ||
        (_wcsicmp(name, L"WinSta0") == 0 && !coordinatedClipboard) || !OpenClipboard(owner)) {
        return {};
    }
    if (GetClipboardOwner() != owner) {
        CloseClipboard();
        return {};
    }
    const auto handle = GetClipboardData(CF_UNICODETEXT);
    const auto bytes = handle ? GlobalSize(handle) : 0;
    const auto* text = bytes >= sizeof(wchar_t) && bytes <= 65536
                           ? static_cast<const wchar_t*>(GlobalLock(handle))
                           : nullptr;
    QString result;
    if (text) {
        const auto capacity = bytes / sizeof(wchar_t);
        std::size_t length = 0;
        while (length < capacity && text[length] != L'\0') {
            ++length;
        }
        if (length < capacity) {
            result = QString::fromWCharArray(text, static_cast<qsizetype>(length));
        }
        GlobalUnlock(handle);
    }
    CloseClipboard();
    return result;
}
} // namespace

class NativeCaptureTests : public QObject {
    Q_OBJECT
  private slots:
    void exactCodesStayInPrivateClipboardAndOutOfHistory() {
        QVERIFY(isolatedClipboard || coordinatedClipboard);
        QTemporaryDir directory;
        const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> owner(
            CreateWindowExW(0, L"STATIC", L"Synthetic clipboard owner", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            nullptr, GetModuleHandleW(nullptr), nullptr),
            DestroyWindow);
        QVERIFY(owner);
        auto monitor = std::make_unique<reposco::NativeClipboardMonitor>();
        auto* source = monitor.get();
        QSignalSpy captured(source, &reposco::ClipboardMonitor::captured);
        reposco::HistoryController history(directory.path(), std::move(monitor));
        QSignalSpy recorded(&history, &reposco::HistoryController::recordRequested);
        QSignalSpy reset(history.model(), &QAbstractItemModel::modelReset);
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        history.setCaptureActive(true);
        QTest::qWait(60);
        for (const auto& text : {QString("123456"), QString(" 123456\r\n")}) {
            const auto expected = recorded.count() + 1;
            QVERIFY(writeSynthetic(owner.get(), text));
            QTRY_COMPARE_WITH_TIMEOUT(recorded.count(), expected, 2000);
            const auto resets = reset.count();
            history.setSearchQuery(history.searchQuery().isEmpty() ? "123456" : "");
            QTRY_VERIFY_WITH_TIMEOUT(reset.count() > resets, 2000);
            QCOMPARE(history.model()->rowCount(), 0);
            QVERIFY(readSynthetic(owner.get()) == text);
        }
        QVERIFY(writeSynthetic(owner.get(), "code 123456"));
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 1, 2000);
        QVERIFY(readSynthetic(owner.get()) == QString("code 123456"));
        const auto captures = captured.count();
        QVERIFY(history.setCapturePolicy(true, false, {QCoreApplication::applicationFilePath()}));
        QTest::qWait(60);
        QVERIFY(writeSynthetic(owner.get(), "excluded native application"));
        QTest::qWait(150);
        QCOMPARE(captured.count(), captures);
        QVERIFY(readSynthetic(owner.get()) == QString("excluded native application"));
        QVERIFY(history.setCapturePolicy(true, false, {}));
        history.selectRow(0);
        const auto beforeRestore = captured.count();
        QSignalSpy restored(&history, &reposco::HistoryController::replayCompleted);
        history.replaySelected(true);
        QTRY_COMPARE_WITH_TIMEOUT(restored.count(), 1, 2000);
        const auto restoreOwner = GetClipboardOwner();
        DWORD restoreProcess = 0;
        QVERIFY(GetWindowThreadProcessId(restoreOwner, &restoreProcess) != 0);
        QCOMPARE(restoreProcess, GetCurrentProcessId());
        QVERIFY(readSynthetic(restoreOwner) == QString("code 123456"));
        QCOMPARE(restored[0][0].toUInt(), GetClipboardSequenceNumber());
        QVERIFY(OpenClipboard(owner.get()));
        const auto excluded =
            RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
        const auto permission = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
        const bool marked =
            IsClipboardFormatAvailable(excluded) && IsClipboardFormatAvailable(permission);
        CloseClipboard();
        QVERIFY(marked);
        QTest::qWait(150);
        QCOMPARE(captured.count(), beforeRestore);
        QCOMPARE(history.model()->rowCount(), 1);
        QSignalSpy stopped(&history, &reposco::HistoryController::stopped);
        history.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
        const auto key = reposco::loadDatabaseKey(directory.path(), true);
        reposco::HistoryStore stored(directory.filePath("history.db").toStdString(), key->bytes());
        QCOMPARE(stored.summaries().size(), std::size_t{1});
        QVERIFY(stored.summaries()[0].excerpt == QString("code 123456"));
        if (coordinatedClipboard) {
            QVERIFY(writeSynthetic(owner.get(), "123456"));
            const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> edit(
                CreateWindowExW(0, L"EDIT", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                GetModuleHandleW(nullptr), nullptr),
                DestroyWindow);
            QVERIFY(edit);
            SendMessageW(edit.get(), WM_PASTE, 0, 0);
            wchar_t pasted[32]{};
            GetWindowTextW(edit.get(), pasted, static_cast<int>(std::size(pasted)));
            QVERIFY(QString::fromWCharArray(pasted) == QString("123456"));
        }
    }
    void syntheticUpdatesRespectMarkersPauseAndBackpressure() {
        QVERIFY(isolatedClipboard);
        const std::unique_ptr<std::remove_pointer_t<HWND>, decltype(&DestroyWindow)> owner(
            CreateWindowExW(0, L"STATIC", L"Synthetic clipboard owner", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            nullptr, GetModuleHandleW(nullptr), nullptr),
            DestroyWindow);
        QVERIFY(owner);
        reposco::NativeClipboardMonitor monitor;
        QSignalSpy ready(&monitor, &reposco::ClipboardMonitor::ready);
        QSignalSpy captured(&monitor, &reposco::ClipboardMonitor::captured);
        QSignalSpy stopped(&monitor, &reposco::ClipboardMonitor::stopped);
        monitor.start();
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 2000);
        monitor.setActive(true, 1);
        QTest::qWait(60);
        QVERIFY(writeSynthetic(owner.get(), "A"));
        QTRY_COMPARE_WITH_TIMEOUT(captured.count(), 1, 2000);
        QCOMPARE(captured[0][0].toString(), QString("A"));
        monitor.acknowledge(captured[0][1].toULongLong());
        const QByteArray zero(4, '\0');
        for (const auto* marker : {L"ExcludeClipboardContentFromMonitorProcessing",
                                   L"Clipboard Viewer Ignore", L"CanIncludeInClipboardHistory"}) {
            QVERIFY(writeSynthetic(owner.get(), "synthetic marked secret", marker, zero));
            QTest::qWait(100);
            QCOMPARE(captured.count(), 1);
        }
        QVERIFY(writeSynthetic(owner.get(), "malformed permission", L"CanIncludeInClipboardHistory",
                               QByteArray(1, '\0')));
        QTest::qWait(100);
        QCOMPARE(captured.count(), 1);
        QVERIFY(writeSynthetic(owner.get(), "B", L"CanUploadToCloudClipboard", zero));
        QTRY_COMPARE_WITH_TIMEOUT(captured.count(), 2, 2000);
        QCOMPARE(captured[1][0].toString(), QString("B"));
        monitor.acknowledge(captured[1][1].toULongLong());
        monitor.setActive(false, 2);
        QTest::qWait(60);
        QVERIFY(writeSynthetic(owner.get(), "paused synthetic"));
        QTest::qWait(100);
        monitor.setActive(true, 3);
        QTest::qWait(100);
        QCOMPARE(captured.count(), 2);
        QVERIFY(writeSynthetic(owner.get(), "C"));
        QTRY_COMPARE_WITH_TIMEOUT(captured.count(), 3, 2000);
        QCOMPARE(captured[2][2].toULongLong(), quint64{3});
        QVERIFY(writeSynthetic(owner.get(), "D"));
        QVERIFY(writeSynthetic(owner.get(), "E"));
        QTest::qWait(100);
        QCOMPARE(captured.count(), 3);
        monitor.acknowledge(captured[2][1].toULongLong());
        QTRY_COMPARE_WITH_TIMEOUT(captured.count(), 4, 2000);
        QCOMPARE(captured[3][0].toString(), QString("E"));
        monitor.stop();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 2000);
    }
    void inactiveListenerStartsAndStopsWithoutReading() {
        reposco::NativeClipboardMonitor monitor;
        QSignalSpy ready(&monitor, &reposco::ClipboardMonitor::ready);
        QSignalSpy stopped(&monitor, &reposco::ClipboardMonitor::stopped);
        QSignalSpy captured(&monitor, &reposco::ClipboardMonitor::captured);
        QSignalSpy failure(&monitor, &reposco::ClipboardMonitor::failure);
        monitor.start();
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 2000);
        QCOMPARE(captured.count(), 0);
        monitor.stop();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 2000);
        QCOMPARE(captured.count(), 0);
        QCOMPARE(failure.count(), 0);
    }
};
int main(int argc, char* argv[]) {
    IsolatedStation isolation;
    if (argc > 2 && std::strcmp(argv[1], "--coordinated-clipboard") == 0) {
        if (qEnvironmentVariable("REPOSCO_COORDINATED_DESKTOP") != "1" ||
            std::strcmp(argv[2], "exactCodesStayInPrivateClipboardAndOutOfHistory") != 0) {
            std::cerr << "Coordinated clipboard test requires explicit desktop authorization\n";
            return 2;
        }
        coordinatedClipboard = true;
        for (int index = 1; index < argc; ++index) {
            argv[index] = argv[index + 1];
        }
        --argc;
    } else if (argc > 1 && std::strcmp(argv[1], "--isolated-clipboard") == 0) {
        if (!isolation.initialize()) {
            std::cerr << "Private clipboard isolation failed: " << GetLastError() << '\n';
            return 2;
        }
        isolatedClipboard = true;
        for (int index = 1; index < argc; ++index) {
            argv[index] = argv[index + 1];
        }
        --argc;
    } else if (argc < 2 ||
               std::strcmp(argv[1], "inactiveListenerStartsAndStopsWithoutReading") != 0) {
        std::cerr << "Active clipboard tests require --isolated-clipboard\n";
        return 2;
    }
    QCoreApplication application(argc, argv);
    NativeCaptureTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "NativeCaptureTests.moc"
