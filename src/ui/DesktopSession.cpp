#include "DesktopSession.h"
#include "HistoryController.h"

#include <QApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QWKQuick/quickwindowagent.h>

namespace reposco {
DesktopSession::DesktopSession(QObject* parent)
    : QObject(parent), show_action_(tr("Show Reposco"), this), quit_action_(tr("Quit"), this),
      capture_action_(this) {
    quit_action_.setObjectName("quitAction");
    connect(&show_action_, &QAction::triggered, this, &DesktopSession::toggleWindow);
    connect(&quit_action_, &QAction::triggered, this, &DesktopSession::quitRequested);
    connect(&hotkey_, &GlobalHotkey::activated, this, &DesktopSession::toggleWindow);
    connect(&engine_, &QQmlEngine::quit, this, &DesktopSession::quitRequested);
    connect(&tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    toggleWindow();
                }
            });
}
DesktopSession::~DesktopSession() = default;
bool DesktopSession::initialize(HistoryController* history) {
    if (window_) {
        return true;
    }
    if (history) {
        engine_.setInitialProperties({{"historyController", QVariant::fromValue(history)}});
        capture_action_.setObjectName("captureAction");
        const auto updateCaptureAction = [this, history] {
            capture_action_.setEnabled(history->available());
            capture_action_.setText(history->captureActive() ? tr("Pause capture")
                                                             : tr("Start / resume text capture"));
        };
        connect(history, &HistoryController::stateChanged, this, updateCaptureAction);
        connect(&capture_action_, &QAction::triggered, history,
                [history] { history->setCaptureActive(!history->captureActive()); });
        updateCaptureAction();
    } else {
        capture_action_.setEnabled(false);
        capture_action_.setText(tr("Capture unavailable"));
    }
    engine_.loadFromModule("Reposco", "Main");
    if (engine_.rootObjects().isEmpty()) {
        return false;
    }
    window_ = qobject_cast<QQuickWindow*>(engine_.rootObjects().constFirst());
    if (!window_) {
        return false;
    }
    window_agent_ = std::make_unique<QWK::QuickWindowAgent>();
    if (!window_agent_->setup(window_)) {
        return false;
    }
    window_agent_->setTitleBar(window_->findChild<QQuickItem*>("titleBar"));
    tray_menu_.addAction(&show_action_);
    tray_menu_.addAction(&capture_action_);
    tray_menu_.addSeparator();
    tray_menu_.addAction(&quit_action_);
    tray_.setIcon(QIcon(":/assets/branding/temporary-icon.svg"));
    tray_.setToolTip(tr("Reposco - encrypted text history"));
    tray_.setContextMenu(&tray_menu_);
    tray_.show();
    if (!hotkey_.registerDefault()) {
        auto* status = window_->findChild<QQuickItem*>("hotkeyStatus");
        if (status) {
            status->setProperty("text",
                                tr("Win+Alt+V unavailable (Windows error %1). Use the tray.")
                                    .arg(hotkey_.errorCode()));
        }
        tray_.showMessage(tr("Reposco"),
                          tr("The global hotkey could not be registered. Use the tray."),
                          QSystemTrayIcon::Warning);
    }
    return true;
}
QQuickWindow* DesktopSession::window() const {
    return window_;
}
bool DesktopSession::trayAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}
unsigned long DesktopSession::hotkeyError() const {
    return hotkey_.errorCode();
}
void DesktopSession::toggleWindow() {
    if (!window_) {
        return;
    }
    if (window_->isVisible()) {
        window_->hide();
    } else {
        const auto geometry = window_->screen()->availableGeometry();
        window_->setPosition(geometry.center() -
                             QPoint(window_->width() / 2, window_->height() / 2));
        window_->show();
        window_->requestActivate();
    }
}
} // namespace reposco
