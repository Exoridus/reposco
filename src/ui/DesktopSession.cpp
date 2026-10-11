#include "DesktopSession.h"
#include "HistoryController.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QWKQuick/quickwindowagent.h>
#include <Windows.h>
#include <array>

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
        openFromHotkey();
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
    history_ = history;
    if (history) {
        connect(history, &HistoryController::replayInvalidated, this,
                [this] { ++paste_generation_; });
        connect(history, &HistoryController::replayCompleted, this,
                [this](quint32 sequence, bool copyOnly) {
                    if (!copyOnly) {
                        pasteRestoredText(sequence);
                    }
                });
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
    connect(window_, &QQuickWindow::visibleChanged, this, [this] {
        qCInfo(reposcoLifecycle) << "palette visible" << window_->isVisible();
        if (!window_->isVisible() && history_ && history_->replayBusy()) {
            history_->cancelReplay();
        }
    });
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
void DesktopSession::openFromHotkey() {
    if (hotkey_open_pending_) {
        return;
    }
    hotkey_open_pending_ = true;
    auto* timer = new QTimer(this);
    timer->setInterval(15);
    auto attempts = std::make_shared<int>(0);
    connect(timer, &QTimer::timeout, this, [this, timer, attempts] {
        bool held = false;
        for (const auto key :
             {VK_LWIN, VK_RWIN, VK_MENU, VK_CONTROL, VK_SHIFT, static_cast<int>('V')}) {
            held = held || (GetAsyncKeyState(key) & 0x8000) != 0;
        }
        // Activating before the global shortcut is released races Windows' Alt-key handling.
        if (held && ++*attempts < 40) {
            return;
        }
        timer->stop();
        timer->deleteLater();
        hotkey_open_pending_ = false;
        showWindow();
    });
    timer->start();
}
void DesktopSession::showWindow() {
    if (!window_) {
        return;
    }
    qCInfo(reposcoLifecycle) << "palette open requested";
    if (!window_->isVisible()) {
        const auto foreground = GetForegroundWindow();
        paste_thread_ = GetWindowThreadProcessId(foreground, &paste_process_);
        paste_target_ = reinterpret_cast<quintptr>(foreground);
        if (paste_process_ == GetCurrentProcessId()) {
            paste_target_ = 0;
        }
        ++paste_generation_;
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
void DesktopSession::replaySelected(bool copyOnly, bool plainText) {
    if (!history_ || !window_->isVisible() || !window_->isActive()) {
        return;
    }
    history_->replaySelected(copyOnly, plainText);
}
void DesktopSession::pasteRestoredText(quint32 sequence) {
    // A queued restore must not steal focus after the user leaves the palette.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    const auto palette = reinterpret_cast<HWND>(window_->winId());
    if (!window_->isVisible() || !window_->isActive() || GetForegroundWindow() != palette ||
        GetClipboardSequenceNumber() != sequence) {
        history_->setReplayStatus(tr("Copied. Focus or clipboard changed; paste manually."));
        return;
    }
    // Windows exposes HWND through Qt's platform-neutral window identifier.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    const auto target = reinterpret_cast<HWND>(paste_target_);
    DWORD processId = 0;
    const auto threadId = GetWindowThreadProcessId(target, &processId);
    bool permitted = target && IsWindow(target) && IsWindowVisible(target) &&
                     processId == paste_process_ && threadId == paste_thread_;
    const auto process =
        permitted ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) : nullptr;
    HANDLE token = nullptr;
    TOKEN_ELEVATION elevation{};
    DWORD returned = 0;
    permitted =
        process && OpenProcessToken(process, TOKEN_QUERY, &token) &&
        GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &returned) &&
        !elevation.TokenIsElevated;
    if (token) {
        CloseHandle(token);
    }
    if (process) {
        CloseHandle(process);
    }
    if (!permitted) {
        history_->setReplayStatus(tr("Copied. No eligible previous window; paste manually."));
        return;
    }
    window_->hide();
    if (!SetForegroundWindow(target)) {
        history_->setReplayStatus(tr("Copied. Target focus unavailable; paste manually."));
        window_->show();
        return;
    }
    const auto generation = ++paste_generation_;
    auto* timer = new QTimer(this);
    timer->setInterval(15);
    auto attempts = std::make_shared<int>(0);
    connect(timer, &QTimer::timeout, this, [this, timer, attempts, generation, sequence, target] {
        const auto finish = [timer] {
            timer->stop();
            timer->deleteLater();
        };
        if (!history_) {
            finish();
            return;
        }
        DWORD processId = 0;
        const auto threadId = GetWindowThreadProcessId(target, &processId);
        if (generation != paste_generation_ || !history_->available() ||
            GetClipboardSequenceNumber() != sequence || GetForegroundWindow() != target ||
            processId != paste_process_ || threadId != paste_thread_) {
            finish();
            history_->setReplayStatus(
                tr("Copied. Paste canceled after focus or clipboard changed."));
            return;
        }
        for (const auto key : {VK_LWIN, VK_RWIN, VK_MENU, VK_CONTROL, VK_SHIFT, VK_RETURN}) {
            if ((GetAsyncKeyState(key) & 0x8000) != 0) {
                if (++*attempts >= 40) {
                    finish();
                    history_->setReplayStatus(
                        tr("Copied. Release shortcut keys and paste manually."));
                }
                return;
            }
        }
        std::array<INPUT, 4> input{};
        constexpr std::array<WORD, 4> keys{VK_CONTROL, 'V', 'V', VK_CONTROL};
        for (std::size_t index = 0; index < input.size(); ++index) {
            input[index].type = INPUT_KEYBOARD;
            input[index].ki.wVk = keys[index];
            input[index].ki.dwFlags = index >= 2 ? KEYEVENTF_KEYUP : 0;
        }
        const auto sent = SendInput(static_cast<UINT>(input.size()), input.data(), sizeof(INPUT));
        if (sent > 0 && sent < input.size()) {
            std::array<INPUT, 2> release{};
            release[0].type = INPUT_KEYBOARD;
            release[0].ki.wVk = 'V';
            release[0].ki.dwFlags = KEYEVENTF_KEYUP;
            release[1].type = INPUT_KEYBOARD;
            release[1].ki.wVk = VK_CONTROL;
            release[1].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(static_cast<UINT>(release.size()), release.data(), sizeof(INPUT));
        }
        finish();
        history_->setReplayStatus(sent == input.size()
                                      ? tr("Paste sent")
                                      : tr("Copied. Windows blocked paste; paste manually."));
    });
    timer->start();
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
