#include "ClipboardFixtures.h"
#include "platform/windows/KeyVault.h"
#include "platform/windows/NativeClipboardMonitor.h"
#include "ui/DesktopSession.h"
#include "ui/HistoryController.h"

#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QLoggingCategory>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QtTest>
#include <Windows.h>
#include <array>

namespace {
QStringList lifecycleMessages;
void lifecycleMessageHandler(QtMsgType, const QMessageLogContext& context, const QString& message) {
    if (QString::fromLatin1(context.category) == QStringLiteral("reposco.lifecycle")) {
        lifecycleMessages.append(message);
    }
}
QQuickItem* findVisualItem(QQuickItem* item, const QString& name) {
    if (item->objectName() == name) {
        return item;
    }
    for (auto* child : item->childItems()) {
        if (auto* found = findVisualItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}
class SyntheticDesktopMonitor final : public reposco::ClipboardMonitor {
  public:
    quint64 generation = 0;
    quint64 restoreToken = 0;
    bool restoreText(const QString&, quint64 token) override {
        restoreToken = token;
        return true;
    }
    void start() override {
        emit ready();
    }
    void setActive(bool, quint64 epoch) override {
        generation = epoch;
    }
    void acknowledge(quint64) override {
    }
    void stop() override {
        emit stopped();
    }
};
} // namespace

class DesktopTests : public QObject {
    Q_OBJECT
  private slots:
    void dispatcherHotkeyAcceptsAbsentResultPointer() {
        reposco::GlobalHotkey hotkey;
        QVERIFY(hotkey.registerDefault());
        QSignalSpy activated(&hotkey, &reposco::GlobalHotkey::activated);
        MSG message{};
        message.message = WM_HOTKEY;
        message.wParam = 0x5250;
        QVERIFY(hotkey.nativeEventFilter("windows_dispatcher_MSG", &message, nullptr));
        QCOMPARE(activated.count(), 1);
    }
    void coordinatedNativeHotkeyKeepsPaletteOpen() {
        if (qEnvironmentVariable("REPOSCO_COORDINATED_DESKTOP") != "1") {
            QSKIP("Requires a currently coordinated interactive desktop.");
        }
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        QCOMPARE(session.hotkeyError(), 0UL);
        QQuickWindow target;
        target.setTitle("Reposco synthetic hotkey target");
        target.resize(320, 180);
        target.show();
        auto* search = session.window()->findChild<QQuickItem*>("paletteSearch");
        QVERIFY(search);
        const auto nativeHandle = [](QQuickWindow* window) {
            // Qt exposes the native HWND through a platform-neutral window identifier.
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            return reinterpret_cast<HWND>(window->winId());
        };
        const auto sendShortcut = [] {
            std::array<INPUT, 6> input{};
            constexpr std::array<WORD, 6> keys{VK_LWIN, VK_MENU, 'V', 'V', VK_MENU, VK_LWIN};
            for (std::size_t index = 0; index < input.size(); ++index) {
                input[index].type = INPUT_KEYBOARD;
                input[index].ki.wVk = keys[index];
                input[index].ki.dwFlags = index >= 3 ? KEYEVENTF_KEYUP : 0;
            }
            return SendInput(static_cast<UINT>(input.size()), input.data(), sizeof(INPUT)) ==
                   input.size();
        };
        for (int iteration = 0; iteration < 3; ++iteration) {
            target.raise();
            target.requestActivate();
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), nativeHandle(&target), 3000);
            for (const auto key : {VK_LWIN, VK_RWIN, VK_MENU, VK_CONTROL, VK_SHIFT}) {
                QVERIFY((GetAsyncKeyState(key) & 0x8000) == 0);
            }
            QVERIFY(sendShortcut());
            QTRY_VERIFY_WITH_TIMEOUT(session.window()->isVisible(), 3000);
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), nativeHandle(session.window()), 3000);
            QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 3000);
            QTest::qWait(500);
            QVERIFY(session.window()->isVisible());
            QVERIFY(session.trayAvailable());
            QVERIFY(sendShortcut());
            QVERIFY(sendShortcut());
            QTest::qWait(300);
            QVERIFY(session.window()->isVisible());
            QCOMPARE(GetForegroundWindow(), nativeHandle(session.window()));
            std::array<INPUT, 2> escape{};
            for (auto& event : escape) {
                event.type = INPUT_KEYBOARD;
                event.ki.wVk = VK_ESCAPE;
            }
            escape[1].ki.dwFlags = KEYEVENTF_KEYUP;
            QCOMPARE(SendInput(static_cast<UINT>(escape.size()), escape.data(), sizeof(INPUT)),
                     static_cast<UINT>(escape.size()));
            QTRY_VERIFY_WITH_TIMEOUT(!session.window()->isVisible(), 3000);
        }
    }
    void delayedReplayDoesNotReclaimFocusFromAnotherWindow() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        history.setCaptureActive(true);
        emit source->captured("synthetic delayed replay", 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 1, 5000);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowActive(session.window()));
        history.selectRow(0);
        QTRY_COMPARE_WITH_TIMEOUT(history.preview(), QString("synthetic delayed replay"), 5000);
        session.replaySelected(false);
        QTRY_VERIFY_WITH_TIMEOUT(source->restoreToken > 0, 5000);
        QQuickWindow other;
        other.setTitle("Reposco synthetic focus change");
        other.resize(320, 180);
        other.show();
        other.requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(&other));
        emit source->restored(source->restoreToken, GetClipboardSequenceNumber(), true);
        QVERIFY(history.replayStatus().contains("Focus or clipboard changed"));
        QVERIFY(other.isActive());
        QVERIFY(session.window()->isVisible());
        history.shutdown();
    }
    void coordinatedTextReplayIntoOwnedNativeFixture() {
        if (qEnvironmentVariable("REPOSCO_COORDINATED_DESKTOP") != "1") {
            QSKIP("Requires a currently coordinated interactive desktop.");
        }
        QTemporaryDir directory;
        const auto original = QString("Synthetic replay\r\nSpacing  preserved  ");
        {
            const auto key = reposco::loadDatabaseKey(directory.path(), false);
            reposco::HistoryStore store(directory.filePath("history.db").toStdString(),
                                        key->bytes());
            const auto content = reposco::normalizeText(original);
            QVERIFY(content);
            store.record(*content, QDateTime::currentMSecsSinceEpoch());
        }
        reposco::HistoryController history(directory.path(),
                                           std::make_unique<reposco::NativeClipboardMonitor>());
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        QCOMPARE(session.hotkeyError(), 0UL);
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        QVERIFY(!history.captureActive());
        QProcess fixture;
        fixture.start(QCoreApplication::applicationFilePath(), {"--paste-fixture"});
        QVERIFY(fixture.waitForStarted(3000));
        QVERIFY(fixture.waitForReadyRead(3000));
        bool valid = false;
        const auto identifier = fixture.readLine().trimmed().toULongLong(&valid);
        QVERIFY(valid);
        // The child returns only its synthetic EDIT handle; no user windows are queried.
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        const auto target = reinterpret_cast<HWND>(static_cast<quintptr>(identifier));
        DWORD processId = 0;
        QVERIFY(GetWindowThreadProcessId(target, &processId) != 0);
        QCOMPARE(static_cast<qint64>(processId), fixture.processId());
        const auto sendKeys = [](std::initializer_list<WORD> keys) {
            std::vector<INPUT> input;
            for (const auto key : keys) {
                INPUT event{};
                event.type = INPUT_KEYBOARD;
                event.ki.wVk = key;
                input.push_back(event);
            }
            const auto pressed = input;
            for (auto it = pressed.rbegin(); it != pressed.rend(); ++it) {
                auto event = *it;
                event.ki.dwFlags = KEYEVENTF_KEYUP;
                input.push_back(event);
            }
            return SendInput(static_cast<UINT>(input.size()), input.data(), sizeof(INPUT)) ==
                   input.size();
        };
        const auto hasPastedOriginal = [&] {
            wchar_t text[256]{};
            DWORD_PTR returned = 0;
            if (!SendMessageTimeoutW(target, WM_GETTEXT, std::size(text),
                                     reinterpret_cast<LPARAM>(text), SMTO_ABORTIFHUNG, 300,
                                     &returned)) {
                return false;
            }
            return QString::fromWCharArray(text) == original;
        };
        for (const int mode : {0, 1, 2}) {
            session.showWindow();
            QVERIFY(QTest::qWaitForWindowActive(session.window()));
            session.window()->hide();
            QVERIFY(SetForegroundWindow(target));
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), target, 3000);
            QVERIFY(sendKeys({VK_LWIN, VK_MENU, 'V'}));
            QTRY_VERIFY_WITH_TIMEOUT(session.window()->isVisible(), 3000);
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            const auto palette = reinterpret_cast<HWND>(session.window()->winId());
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), palette, 3000);
            QTRY_VERIFY_WITH_TIMEOUT(session.window()->isActive(), 3000);
            history.selectRow(0);
            QTRY_COMPARE_WITH_TIMEOUT(history.preview(), original, 3000);
            auto* search = session.window()->findChild<QQuickItem*>("paletteSearch");
            QVERIFY(search);
            QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 3000);
            if (mode == 0) {
                QVERIFY(sendKeys({VK_CONTROL, VK_RETURN}));
                QTRY_COMPARE_WITH_TIMEOUT(history.replayStatus(), QString("Copied text"), 3000);
                QVERIFY(session.window()->isVisible());
                QVERIFY(!hasPastedOriginal());
                SendMessageW(target, WM_PASTE, 0, 0);
            } else if (mode == 1) {
                QVERIFY(sendKeys({VK_RETURN}));
            } else {
                QVERIFY(sendKeys({VK_SHIFT, VK_RETURN}));
            }
            QTRY_VERIFY2_WITH_TIMEOUT(
                hasPastedOriginal(),
                qPrintable(QString("Mode %1: %2").arg(mode).arg(history.replayStatus())), 3000);
            if (mode != 0) {
                QVERIFY(!session.window()->isVisible());
                QCOMPARE(history.replayStatus(), QString("Paste sent"));
            }
            QCOMPARE(history.model()->rowCount(), 1);
            SendMessageW(target, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
        }
        history.shutdown();
        fixture.kill();
        QVERIFY(fixture.waitForFinished(3000));
    }
    void syntheticVisualStatesAndKeyboardSelection() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowExposed(session.window()));
        const auto output = qEnvironmentVariable("REPOSCO_VISUAL_OUTPUT");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
        }
        const auto snapshot = [&](const QString& name) {
            QTest::qWait(150);
            const auto image = session.window()->grabWindow();
            QVERIFY(!image.isNull());
            if (!output.isEmpty()) {
                QVERIFY(image.save(output + "/" + name + ".png"));
            }
        };
        snapshot("empty-dark");
        session.window()->setProperty("darkMode", false);
        snapshot("empty-light");
        history.setCaptureActive(true);
        quint64 token = 0;
        for (const auto* text : {"Meeting notes: synthetic fixtures only", "Architecture notes",
                                 "https://github.com/Exoridus",
                                 "const settings = {\n  encrypted: true,\n  history: true\n};"}) {
            emit source->captured(text, ++token, source->generation);
            auto* list = session.window()->findChild<QQuickItem*>("historyList");
            QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), static_cast<int>(token),
                                      5000);
        }
        history.setCaptureActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!history.preview().isEmpty(), 5000);
        session.window()->setProperty("darkMode", true);
        snapshot("populated-dark");
        auto* search = session.window()->findChild<QQuickItem*>("paletteSearch");
        auto* list = session.window()->findChild<QQuickItem*>("historyList");
        session.focusSearch();
        QTest::keyClick(session.window(), Qt::Key_Down);
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        QTest::keyClick(session.window(), Qt::Key_Up);
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        search->setProperty("text", "architecture");
        QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(history.preview(), QString("Architecture notes"), 5000);
        snapshot("search-dark");
        session.window()->setProperty("darkMode", false);
        snapshot("search-light");
        search->setProperty("text", "");
        QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), 4, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(history.preview().startsWith("const settings"), 5000);
        snapshot("populated-light");
        history.setCaptureActive(true);
        reposco::ClipboardContent rich;
        rich.text = "Item\tCount\nAlpha\t2";
        rich.html = reposco::testing::htmlClipboard(
            "<table><tr><th>Item</th><th>Count</th></tr><tr><td>Alpha</td><td>2</td></tr></table>");
        emit source->capturedContent(rich, ++token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 5, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(history.previewKind(), QString("Table"), 5000);
        snapshot("table-light");
        session.window()->setProperty("darkMode", true);
        snapshot("table-dark");
        rich = {};
        rich.text = "# Synthetic heading\n\n**Offline preview**\n\n- original content preserved";
        emit source->capturedContent(rich, ++token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 6, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!history.darkPreviewImage().isEmpty(), 5000);
        snapshot("markdown-dark");
        session.window()->setProperty("darkMode", false);
        snapshot("markdown-light");
        rich = {};
        QImage fixture(120, 72, QImage::Format_ARGB32);
        fixture.fill(QColor(50, 130, 210));
        QBuffer buffer(&rich.png);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(fixture.save(&buffer, "PNG"));
        emit source->capturedContent(rich, ++token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 7, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(history.previewKind(), QString("Image"), 5000);
        snapshot("image-light");
        session.window()->setProperty("darkMode", true);
        auto* previewImage = session.window()->findChild<QQuickItem*>("previewImage");
        QVERIFY(previewImage);
        QTest::qWait(200);
        const auto painted = session.window()->grabWindow();
        const auto scale = qreal(painted.width()) / session.window()->width();
        const auto origin = previewImage->mapToScene(QPointF{});
        const int scanY = int((origin.y() + previewImage->height() / 2) * scale);
        int visiblePixels = 0;
        for (int x = int(origin.x() * scale); x < int((origin.x() + previewImage->width()) * scale);
             ++x) {
            if (painted.pixelColor(x, scanY) == QColor(50, 130, 210)) {
                ++visiblePixels;
            }
        }
        QVERIFY2(visiblePixels >= previewImage->property("paintedWidth").toReal() * scale * 0.8,
                 "Image preview is clipped despite fitting its viewport");
        snapshot("image-dark");
        history.setCaptureActive(false);
        session.window()->setProperty("darkMode", false);
        QMetaObject::invokeMethod(session.window(), "openSettings");
        auto* settings = session.window()->findChild<QQuickWindow*>("settingsWindow");
        QVERIFY(settings);
        QVERIFY(QTest::qWaitForWindowExposed(settings));
        QTest::qWait(150);
        if (!output.isEmpty()) {
            QVERIFY(settings->grabWindow().save(output + "/settings-light.png"));
        }
        settings->hide();
        session.showWindow();
        QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(session.window()->isActive(), 2000);
        QTest::keyClick(session.window(), Qt::Key_Escape);
        QVERIFY(!session.window()->isVisible());
        session.showWindow();
        QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 2000);
        QSignalSpy stopped(&history, &reposco::HistoryController::stopped);
        history.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void nativeHotkeyDispatchIsIdempotentAndDiagnosticsContainNoPayload() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        auto* hotkey = session.findChild<reposco::GlobalHotkey*>();
        QVERIFY(hotkey);
        QVERIFY(session.setShortcut("Win+Alt+V"));
        QVERIFY(!session.setShortcut("unsupported"));
        MSG message{};
        message.message = WM_HOTKEY;
        message.wParam = 0x5250;
        qintptr result = -1;
        lifecycleMessages.clear();
        QLoggingCategory::setFilterRules(QStringLiteral("reposco.lifecycle.info=true"));
        const auto previous = qInstallMessageHandler(lifecycleMessageHandler);
        for (int index = 0; index < 6; ++index) {
            hotkey->nativeEventFilter("windows_generic_MSG", &message, &result);
        }
        session.window()->hide();
        hotkey->nativeEventFilter("windows_dispatcher_MSG", &message, &result);
        qInstallMessageHandler(previous);
        QLoggingCategory::setFilterRules(QString{});
        QTRY_VERIFY_WITH_TIMEOUT(session.window()->isVisible(), 2000);
        QCOMPARE(result, qintptr{0});
        QVERIFY(!lifecycleMessages.empty());
        for (const auto& line : lifecycleMessages) {
            QVERIFY(line == "palette open requested" || line.startsWith("palette visible ") ||
                    line.startsWith("palette active ") || line == "focus search" ||
                    line == "hotkey activation");
        }
        QTest::qWait(350);
        QVERIFY(session.window()->isVisible());
        session.window()->close();
        QVERIFY(!session.window()->isVisible());
    }
    void paletteSearchAndSeparateSettingsExist() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        session.showWindow();
        auto* search = session.window()->findChild<QQuickItem*>("paletteSearch");
        QVERIFY(search);
        QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 2000);
        auto* empty = session.window()->findChild<QQuickItem*>("emptyState");
        QVERIFY(empty);
        QVERIFY(empty->isVisible());
        QVERIFY(session.window()->width() <= 700);
        QVERIFY(session.window()->height() <= 480);
        QVERIFY(QMetaObject::invokeMethod(session.window(), "openSettings"));
        auto* settings = session.window()->findChild<QQuickWindow*>("settingsWindow");
        QVERIFY(settings);
        QVERIFY(settings != session.window());
        QVERIFY(settings->isVisible());
        settings->hide();
        session.showWindow();
        session.window()->setProperty("darkMode", false);
        QVERIFY(!session.window()->grabWindow().isNull());
        session.window()->setProperty("darkMode", true);
        session.window()->setProperty("reducedMotion", true);
        QVERIFY(!session.window()->grabWindow().isNull());
        session.window()->close();
        QVERIFY(!session.window()->isVisible());
        session.showWindow();
        QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 2000);
    }
    void repeatedShowRequestsKeepPaletteVisible() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        QAction* show = nullptr;
        for (auto* action : session.findChildren<QAction*>()) {
            if (action->text() == "Show Reposco") {
                show = action;
            }
        }
        QVERIFY(show);
        show->trigger();
        QVERIFY(session.window()->isVisible());
        for (int index = 0; index < 5; ++index) {
            show->trigger();
            QVERIFY(session.window()->isVisible());
        }
        session.window()->close();
        QVERIFY(!session.window()->isVisible());
        show->trigger();
        QVERIFY(session.window()->isVisible());
    }
    void historyControlsAndPlainTextList() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        session.window()->show();
        QVERIFY(QTest::qWaitForWindowExposed(session.window()));
        auto* toggle = session.window()->findChild<QQuickItem*>("captureToggle");
        QVERIFY(toggle);
        QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
        QVERIFY(history.captureActive());
        emit source->captured("<img src='http://127.0.0.1/secret'> synthetic", 1,
                              source->generation);
        auto* list = session.window()->findChild<QQuickItem*>("historyList");
        QVERIFY(list);
        QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(list, "historyExcerpt"), 2000);
        auto* excerpt = findVisualItem(list, "historyExcerpt");
        QCOMPARE(excerpt->property("textFormat").toInt(), 0);
        QVERIFY(excerpt->property("text").toString().startsWith("<img"));
        auto* pause = session.findChild<QAction*>("captureAction");
        QVERIFY(pause);
        pause->trigger();
        QVERIFY(!history.captureActive());
        QSignalSpy stopped(&history, &reposco::HistoryController::stopped);
        history.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void windowTrayAndShutdown() {
        QApplication::setQuitOnLastWindowClosed(false);
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        QVERIFY(session.window());
        QCOMPARE(session.window()->title(), QString("Reposco"));
        session.window()->show();
        QVERIFY(QTest::qWaitForWindowExposed(session.window()));
        QVERIFY(!session.window()->grabWindow().isNull());
        session.window()->close();
        QVERIFY(!session.window()->isVisible());
        QVERIFY(!QApplication::quitOnLastWindowClosed());
        session.toggleWindow();
        QVERIFY(session.window()->isVisible());
        session.toggleWindow();
        QVERIFY(!session.window()->isVisible());
        auto* quitAction = session.findChild<QAction*>("quitAction");
        QVERIFY(quitAction);
        QSignalSpy quitSpy(&session, &reposco::DesktopSession::quitRequested);
        quitAction->trigger();
        QCOMPARE(quitSpy.count(), 1);
        QVERIFY(session.trayAvailable());
    }
};

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    if (application.arguments().contains("--paste-fixture")) {
        const auto edit =
            CreateWindowExW(0, L"EDIT", L"", WS_OVERLAPPEDWINDOW | WS_VISIBLE | ES_MULTILINE, 100,
                            100, 380, 180, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!edit) {
            return 2;
        }
        QTextStream(stdout) << reinterpret_cast<quintptr>(edit) << Qt::endl;
        QTimer focus;
        focus.setInterval(20);
        QObject::connect(&focus, &QTimer::timeout, &application, [edit] {
            if (GetForegroundWindow() == edit && GetFocus() != edit) {
                SetFocus(edit);
            }
        });
        focus.start();
        QTimer::singleShot(30000, &application, &QCoreApplication::quit);
        const auto result = application.exec();
        DestroyWindow(edit);
        return result;
    }
    DesktopTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "DesktopTests.moc"
