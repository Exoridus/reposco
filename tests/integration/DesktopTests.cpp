#include "ui/DesktopSession.h"
#include "ui/HistoryController.h"

#include <QApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
#include <Windows.h>

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
        QVERIFY(session.window()->isVisible());
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

QTEST_MAIN(DesktopTests)
#include "DesktopTests.moc"
