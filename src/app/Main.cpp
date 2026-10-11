#include "ui/DesktopSession.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("Reposco");
    QApplication::setOrganizationName("Reposco");
    QApplication::setApplicationVersion(REPOSCO_VERSION);
    QApplication::setWindowIcon(QIcon(":/assets/branding/temporary-icon.svg"));
    QApplication::setQuitOnLastWindowClosed(false);
    QQuickWindow::setDefaultAlphaBuffer(true);
    reposco::DesktopSession session;
    if (!session.initialize()) {
        return 1;
    }
    QObject::connect(&session, &reposco::DesktopSession::quitRequested, &application,
                     &QApplication::quit);
    if (application.arguments().contains("--show") || !session.trayAvailable()) {
        session.toggleWindow();
    }
    if (application.arguments().contains("--smoke")) {
        session.window()->show();
        QTimer::singleShot(1500, &session, [&session] {
            if (auto* quitAction = session.findChild<QAction*>("quitAction")) {
                quitAction->trigger();
            } else {
                QApplication::exit(2);
            }
        });
    }
    return application.exec();
}
