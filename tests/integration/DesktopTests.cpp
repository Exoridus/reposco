#include "ui/DesktopSession.h"
#include "ui/HistoryController.h"

#include <QApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

namespace {
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
