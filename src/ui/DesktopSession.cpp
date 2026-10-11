#include "DesktopSession.h"
#include "HistoryController.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QWKQuick/quickwindowagent.h>
#include <Windows.h>

Q_LOGGING_CATEGORY(reposcoLifecycle, "reposco.lifecycle", QtWarningMsg)

namespace reposco {
DesktopSession::DesktopSession(QObject* parent)
    : QObject(parent), show_action_(tr("Show Reposco"), this), quit_action_(tr("Quit"), this),
      capture_action_(this), hotkey_(this) {
    show_action_.setObjectName("showAction");
    quit_action_.setObjectName("quitAction");
    connect(&show_action_, &QAction::triggered, this, &DesktopSession::showWindow);
    connect(&quit_action_, &QAction::triggered, this, &DesktopSession::quitRequested);
    connect(&hotkey_, &GlobalHotkey::activated, this, [this] {
        qCInfo(reposcoLifecycle) << "hotkey activation";
        showWindow();
    });
    connect(&engine_, &QQmlEngine::quit, this, &DesktopSession::quitRequested);
    connect(&tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    qCInfo(reposcoLifecycle) << "tray activation";
                    showWindow();
                }
            });
}
DesktopSession::~DesktopSession() = default;
bool DesktopSession::initialize(HistoryController* history) {
    if (window_) {
        return true;
    }
    engine_.setInitialProperties({{"desktopSession", QVariant::fromValue(this)}});
    if (history) {
        engine_.setInitialProperties({{"historyController", QVariant::fromValue(history)},
                                      {"desktopSession", QVariant::fromValue(this)}});
        capture_action_.setObjectName("captureAction");
        const auto updateCaptureAction = [this, history] {
            capture_action_.setEnabled(history->available());
            capture_action_.setText(history->captureActive()
                                        ? tr("Pause capture")
                                        : tr("Enable / resume in Settings..."));
        };
        connect(history, &HistoryController::stateChanged, this, updateCaptureAction);
        connect(&capture_action_, &QAction::triggered, history, [this, history] {
            if (history->captureActive()) {
                history->setCaptureActive(false);
            } else {
                QMetaObject::invokeMethod(window_, "openSettings");
            }
        });
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
    connect(window_, &QQuickWindow::visibleChanged, this,
            [this] { qCInfo(reposcoLifecycle) << "palette visible" << window_->isVisible(); });
    connect(window_, &QQuickWindow::activeChanged, this, [this] {
        qCInfo(reposcoLifecycle) << "palette active" << window_->isActive();
        if (window_->isActive() && window_->isVisible()) {
            focusSearch();
        }
    });
    window_agent_ = std::make_unique<QWK::QuickWindowAgent>();
    if (!window_agent_->setup(window_)) {
        return false;
    }
    window_agent_->setTitleBar(window_->findChild<QQuickItem*>("titleBar"));
    window_agent_->setHitTestVisible(window_->findChild<QQuickItem*>("settingsButton"), true);
    tray_menu_.addAction(&show_action_);
    auto* settingsAction = tray_menu_.addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this,
            [this] { QMetaObject::invokeMethod(window_, "openSettings"); });
    auto* clearAction = tray_menu_.addAction(tr("Clear history..."));
    connect(clearAction, &QAction::triggered, this,
            [this] { QMetaObject::invokeMethod(window_, "confirmClearHistory"); });
    tray_menu_.addAction(&capture_action_);
    tray_menu_.addSeparator();
    tray_menu_.addAction(&quit_action_);
    tray_.setIcon(QIcon(":/assets/branding/reposco.ico"));
    tray_.setToolTip(tr("Reposco - encrypted text history"));
    tray_.setContextMenu(&tray_menu_);
    tray_.show();
    if (!hotkey_.registerDefault()) {
        shortcut_status_ = tr("Shortcut unavailable. Use the tray or choose another combination.");
        emit shortcutChanged();
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
const QString& DesktopSession::shortcut() const {
    return shortcut_;
}
const QString& DesktopSession::shortcutStatus() const {
    return shortcut_status_;
}
bool DesktopSession::setShortcut(const QString& shortcut) {
    const auto modifiers = [](const QString& value) -> unsigned int {
        if (value == QStringLiteral("Win+Alt+V")) {
            return MOD_WIN | MOD_ALT;
        }
        if (value == QStringLiteral("Ctrl+Alt+V")) {
            return MOD_CONTROL | MOD_ALT;
        }
        if (value == QStringLiteral("Ctrl+Shift+V")) {
            return MOD_CONTROL | MOD_SHIFT;
        }
        return 0;
    };
    const auto requested = modifiers(shortcut);
    if (!requested) {
        return false;
    }
    if (!hotkey_.registerShortcut(requested, 'V')) {
        const bool restored = hotkey_.registerShortcut(modifiers(shortcut_), 'V');
        shortcut_status_ = restored ? tr("Shortcut conflict. Previous shortcut restored.")
                                    : tr("Shortcut unavailable. Use the tray.");
        emit shortcutChanged();
        return false;
    }
    shortcut_ = shortcut;
    shortcut_status_ = tr("Shortcut registered for this session.");
    emit shortcutChanged();
    return true;
}
void DesktopSession::focusSearch() {
    if (auto* search = window_->findChild<QQuickItem*>("paletteSearch")) {
        search->forceActiveFocus(Qt::ShortcutFocusReason);
        qCInfo(reposcoLifecycle) << "focus search";
    }
}
void DesktopSession::showWindow() {
    if (!window_) {
        return;
    }
    qCInfo(reposcoLifecycle) << "palette open requested";
    if (!window_->isVisible()) {
        const auto foreground = GetForegroundWindow();
        const auto monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        QScreen* target = window_->screen();
        if (GetMonitorInfoW(monitor, &info)) {
            for (auto* screen : QGuiApplication::screens()) {
                if (screen->name() == QString::fromWCharArray(info.szDevice)) {
                    target = screen;
                    break;
                }
            }
        }
        window_->setScreen(target);
        const auto geometry = target->availableGeometry();
        window_->setWidth(qMin(680, geometry.width()));
        if (window_->height() > geometry.height()) {
            window_->setHeight(geometry.height());
        }
        window_->setPosition(geometry.center() -
                             QPoint(window_->width() / 2, window_->height() / 2));
        window_->show();
    }
    window_->raise();
    window_->requestActivate();
    focusSearch();
}
void DesktopSession::toggleWindow() {
    if (!window_) {
        return;
    }
    if (window_->isVisible()) {
        window_->hide();
    } else {
        showWindow();
    }
}
} // namespace reposco
