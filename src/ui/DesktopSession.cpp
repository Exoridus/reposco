#include "DesktopSession.h"
#include "HistoryController.h"

#include <QApplication>
#include <QLoggingCategory>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QStyleHints>
#include <QTimer>
#include <QWKQuick/quickwindowagent.h>
#include <Windows.h>
#include <array>
#include <dwmapi.h>

Q_LOGGING_CATEGORY(reposcoLifecycle, "reposco.lifecycle", QtWarningMsg)

namespace reposco {
DesktopSession::DesktopSession(QObject* parent)
    : QObject(parent), show_action_(tr("Open History"), this), quit_action_(tr("Quit"), this),
      capture_action_(this), hotkey_(this) {
    show_action_.setObjectName("showAction");
    quit_action_.setObjectName("quitAction");
    connect(&show_action_, &QAction::triggered, this, &DesktopSession::showWorkspace);
    connect(&quit_action_, &QAction::triggered, this, &DesktopSession::quitRequested);
    connect(&hotkey_, &GlobalHotkey::activated, this, [this] {
        qCInfo(reposcoLifecycle) << "hotkey activation";
        openFromHotkey();
    });
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            &DesktopSession::updateMaterial);
    connect(&engine_, &QQmlEngine::quit, this, &DesktopSession::quitRequested);
    connect(&tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                    qCInfo(reposcoLifecycle) << "tray activation";
                    showWorkspace();
                }
            });
}
DesktopSession::~DesktopSession() {
    QCoreApplication::instance()->removeNativeEventFilter(this);
}
bool DesktopSession::initialize(HistoryController* history) {
    if (window_) {
        return true;
    }
    engine_.setInitialProperties({{"desktopSession", QVariant::fromValue(this)}});
    history_ = history;
    if (history) {
        connect(history, &HistoryController::replayInvalidated, this, [this] {
            ++paste_generation_;
            replay_origin_.clear();
        });
        connect(history, &HistoryController::replayCompleted, this,
                [this](quint32 sequence, bool copyOnly) {
                    if (replay_origin_ == window_) {
                        if (copyOnly) {
                            window_->hide();
                        } else {
                            pasteRestoredText(sequence);
                        }
                    }
                    replay_origin_.clear();
                });
        engine_.setInitialProperties({{"historyController", QVariant::fromValue(history)},
                                      {"desktopSession", QVariant::fromValue(this)}});
        capture_action_.setObjectName("captureAction");
        const auto updateCaptureAction = [this, history] {
            capture_action_.setEnabled(history->available() && !history->policyPending());
            capture_action_.setText(history->captureActive()
                                        ? tr("Pause capture")
                                        : tr("Enable / resume in Settings..."));
        };
        connect(history, &HistoryController::stateChanged, this, updateCaptureAction);
        connect(&capture_action_, &QAction::triggered, history, [this, history] {
            if (history->captureActive()) {
                history->setCaptureActive(false);
            } else {
                showSettings(true);
            }
        });
        updateCaptureAction();
    } else {
        capture_action_.setEnabled(false);
        capture_action_.setText(tr("Capture unavailable"));
    }
    QQuickWindow::setDefaultAlphaBuffer(true);
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
        if (!window_->isVisible()) {
            if (history_ && history_->replayBusy() && replay_origin_ == window_) {
                history_->cancelReplay();
            }
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
    // The transient picker is entirely client content; it has no draggable title region.
    connect(window_, SIGNAL(darkModeChanged()), this, SLOT(updateMaterial()));
    auto* tokens = window_->findChild<QObject*>("paletteStyle");
    if (!tokens) {
        return false;
    }
    // Shared tokens must survive until both engine-owned root windows are destroyed.
    style_tokens_ = tokens;
    style_tokens_->setParent(&engine_);
    engine_.setInitialProperties({{"desktopSession", QVariant::fromValue(this)},
                                  {"historyController", QVariant::fromValue(history)},
                                  {"tokens", QVariant::fromValue(tokens)}});
    engine_.loadFromModule("Reposco", "WorkspaceWindow");
    if (engine_.rootObjects().size() != 2) {
        return false;
    }
    workspace_window_ = qobject_cast<QQuickWindow*>(engine_.rootObjects().constLast());
    if (!workspace_window_) {
        return false;
    }
    workspace_agent_ = std::make_unique<QWK::QuickWindowAgent>();
    if (!workspace_agent_->setup(workspace_window_)) {
        return false;
    }
    workspace_agent_->setTitleBar(workspace_window_->findChild<QQuickItem*>("workspaceTitleBar"));
    for (const auto* name : {"workspaceStatus", "workspaceSettings", "workspaceClose"}) {
        if (auto* item = workspace_window_->findChild<QQuickItem*>(name)) {
            workspace_agent_->setHitTestVisible(item, true);
        }
    }
    connect(workspace_window_, &QQuickWindow::visibleChanged, this, [this] {
        if (!workspace_window_->isVisible() && history_ && history_->replayBusy() &&
            replay_origin_ == workspace_window_) {
            history_->cancelReplay();
        }
    });
    for (auto* surface : {window_, workspace_window_}) {
        connect(surface, &QQuickWindow::heightChanged, this, [this, surface] {
            if (surface->isVisible()) {
                fitWindow(surface, false);
            }
        });
    }
    QCoreApplication::instance()->installNativeEventFilter(this);
    updateMaterial();
    tray_menu_.addAction(&show_action_);
    auto* settingsAction = tray_menu_.addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, [this] { showSettings(); });
    auto* clearAction = tray_menu_.addAction(tr("Clear history..."));
    connect(clearAction, &QAction::triggered, this, [this] {
        showWorkspace();
        QMetaObject::invokeMethod(workspace_window_, "confirmClearHistory");
    });
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
QObject* DesktopSession::styleTokens() const {
    return style_tokens_;
}
QQuickWindow* DesktopSession::workspaceWindow() const {
    return workspace_window_;
}
bool DesktopSession::micaActive() const {
    return mica_active_;
}
QColor DesktopSession::systemAccent() const {
    return QApplication::palette().color(QPalette::Accent);
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
bool DesktopSession::acrylicActive() const {
    return acrylic_active_;
}
bool DesktopSession::systemDark() const {
    return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
}
bool DesktopSession::highContrast() const {
    return high_contrast_;
}
bool DesktopSession::systemReducedMotion() const {
    return system_reduced_motion_;
}
bool DesktopSession::acrylicEnabled() const {
    return acrylic_enabled_;
}
void DesktopSession::setAcrylicEnabled(bool enabled) {
    if (acrylic_enabled_ != enabled) {
        acrylic_enabled_ = enabled;
        updateMaterial();
    }
}
void DesktopSession::updateMaterial() {
    if (!window_agent_ || !window_) {
        return;
    }
    HIGHCONTRASTW contrast{};
    contrast.cbSize = sizeof(contrast);
    const bool contrastKnown =
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    high_contrast_ = !contrastKnown || (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    BOOL animations = FALSE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    system_reduced_motion_ = !animations;
    DWORD transparency = 0;
    DWORD size = sizeof(transparency);
    const auto transparencyResult = RegGetValueW(
        HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"EnableTransparency", RRF_RT_REG_DWORD, nullptr, &transparency, &size);
    const bool transparencyEnabled = transparencyResult == ERROR_FILE_NOT_FOUND ||
                                     (transparencyResult == ERROR_SUCCESS && transparency != 0);
    const bool permitted = acrylic_enabled_ && !high_contrast_ && transparencyEnabled;
    const auto apply = [permitted](QQuickWindow* surface, QWK::QuickWindowAgent* agent,
                                   const QString& attribute, DWM_SYSTEMBACKDROP_TYPE expected) {
        agent->setWindowAttribute(QStringLiteral("dark-mode"),
                                  surface->property("darkMode").toBool());
        const bool requested = agent->setWindowAttribute(attribute, permitted);
        // QWindowKit reports dispatch, not the DWM HRESULT. Verify the applied backend.
        // Qt's native window identifier represents HWND on Windows.
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        const auto handle = reinterpret_cast<HWND>(surface->winId());
        DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_NONE;
        const auto result =
            DwmGetWindowAttribute(handle, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
        const bool active = permitted && requested && SUCCEEDED(result) && backdrop == expected;
        if (!active) {
            agent->setWindowAttribute(attribute, false);
        }
        const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
        DwmSetWindowAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
        return active;
    };
    acrylic_active_ = apply(window_, window_agent_.get(), QStringLiteral("acrylic-material"),
                            DWMSBT_TRANSIENTWINDOW);
    mica_active_ =
        workspace_window_ && workspace_agent_ &&
        apply(workspace_window_, workspace_agent_.get(), QStringLiteral("mica"), DWMSBT_MAINWINDOW);
    emit materialChanged();
}
bool DesktopSession::nativeEventFilter(const QByteArray& eventType, void* message,
                                       qintptr* result) {
    Q_UNUSED(result)
    if (eventType != "windows_generic_MSG" || !message) {
        return false;
    }
    const auto* native = static_cast<MSG*>(message);
    if ((native->message == WM_SETTINGCHANGE || native->message == WM_THEMECHANGED ||
         native->message == WM_DWMCOMPOSITIONCHANGED) &&
        !material_update_pending_) {
        material_update_pending_ = true;
        QTimer::singleShot(0, this, [this] {
            material_update_pending_ = false;
            updateMaterial();
        });
    }
    return false;
}
void DesktopSession::focusSearch() {
    QMetaObject::invokeMethod(window_, "focusCurrentView");
    if (!window_->property("settingsOpen").toBool()) {
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
        window_->resize(600, 334);
        fitWindow(window_, true);
        window_->show();
    }
    window_->raise();
    window_->requestActivate();
    focusSearch();
}
void DesktopSession::fitWindow(QQuickWindow* surface, bool center) {
    if (!surface || !surface->screen()) {
        return;
    }
    const auto area = surface->screen()->availableGeometry().adjusted(16, 16, -16, -16);
    const QSize bounded(qMin(surface->width(), area.width()),
                        qMin(surface->height(), area.height()));
    if (surface->size() != bounded) {
        surface->resize(bounded);
    }
    if (center) {
        surface->setPosition(area.center() - QPoint(surface->width() / 2, surface->height() / 2));
    } else {
        surface->setPosition(qBound(area.left(), surface->x(),
                                    qMax(area.left(), area.right() + 1 - surface->width())),
                             qBound(area.top(), surface->y(),
                                    qMax(area.top(), area.bottom() + 1 - surface->height())));
    }
}
void DesktopSession::showWorkspace() {
    if (!workspace_window_) {
        return;
    }
    window_->hide();
    const bool opening = !workspace_window_->isVisible();
    if (opening) {
        fitWindow(workspace_window_, true);
        workspace_window_->show();
    }
    workspace_window_->raise();
    workspace_window_->requestActivate();
    if (opening) {
        QMetaObject::invokeMethod(workspace_window_, "focusCurrentView");
    }
}
void DesktopSession::showSettings(bool confirmCapture) {
    showWorkspace();
    QMetaObject::invokeMethod(workspace_window_, "openSettings");
    if (confirmCapture) {
        QMetaObject::invokeMethod(workspace_window_, "confirmCapture");
    }
}
void DesktopSession::copySelected(bool plainText) {
    if (!history_ || !history_->available() || history_->replayBusy() ||
        !workspace_window_->isVisible() || !workspace_window_->isActive() ||
        workspace_window_->property("settingsOpen").toBool()) {
        return;
    }
    replay_origin_ = workspace_window_;
    history_->replaySelected(true, plainText);
}
void DesktopSession::replaySelected(bool copyOnly, bool plainText) {
    if (!history_ || !history_->available() || history_->replayBusy() || !window_->isVisible() ||
        !window_->isActive()) {
        return;
    }
    replay_origin_ = window_;
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
