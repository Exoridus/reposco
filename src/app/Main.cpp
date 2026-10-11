#include "platform/windows/NativeClipboardMonitor.h"
#include "ui/DesktopSession.h"
#include "ui/HistoryController.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setApplicationName("Reposco");
    QApplication::setOrganizationName("Reposco");
    QApplication::setApplicationVersion(REPOSCO_VERSION);
    QApplication::setWindowIcon(QIcon(":/assets/branding/reposco.ico"));
    QApplication::setQuitOnLastWindowClosed(false);
    QQuickWindow::setDefaultAlphaBuffer(true);
    const bool smoke = application.arguments().contains("--smoke");
    std::unique_ptr<QTemporaryDir> smokeDirectory;
    QString dataDirectory;
    if (smoke) {
        smokeDirectory = std::make_unique<QTemporaryDir>();
        if (!smokeDirectory->isValid()) {
            return 2;
        }
        dataDirectory = smokeDirectory->path();
    } else {
        dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    reposco::HistoryController history(dataDirectory,
                                       std::make_unique<reposco::NativeClipboardMonitor>());
    reposco::DesktopSession session;
    if (!session.initialize(&history)) {
        return 1;
    }
    QObject::connect(&session, &reposco::DesktopSession::quitRequested, &history,
                     &reposco::HistoryController::shutdown);
    QObject::connect(&history, &reposco::HistoryController::stopped, &application,
                     &QApplication::quit);
    history.start();
    if (application.arguments().contains("--show") || !session.trayAvailable()) {
        session.toggleWindow();
    }
    if (smoke) {
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
