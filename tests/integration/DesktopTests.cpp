#include "ui/DesktopSession.h"

#include <QApplication>
#include <QQuickWindow>
#include <QtTest>

class DesktopTests : public QObject {
    Q_OBJECT
  private slots:
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
