#pragma once

#include "platform/windows/GlobalHotkey.h"

#include <QAbstractNativeEventFilter>
#include <QAction>
#include <QColor>
#include <QMenu>
#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <memory>

namespace QWK {
class QuickWindowAgent;
}

namespace reposco {
class HistoryController;
class DesktopSession final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(QObject* styleTokens READ styleTokens CONSTANT)
    Q_PROPERTY(QQuickWindow* workspaceWindow READ workspaceWindow CONSTANT)
    Q_PROPERTY(bool micaActive READ micaActive NOTIFY materialChanged)
    Q_PROPERTY(QColor systemAccent READ systemAccent NOTIFY materialChanged)
    Q_PROPERTY(bool acrylicActive READ acrylicActive NOTIFY materialChanged)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY materialChanged)
    Q_PROPERTY(bool highContrast READ highContrast NOTIFY materialChanged)
    Q_PROPERTY(bool systemReducedMotion READ systemReducedMotion NOTIFY materialChanged)
    Q_PROPERTY(
        bool acrylicEnabled READ acrylicEnabled WRITE setAcrylicEnabled NOTIFY materialChanged)
    Q_PROPERTY(QString shortcut READ shortcut NOTIFY shortcutChanged)
    Q_PROPERTY(QString shortcutStatus READ shortcutStatus NOTIFY shortcutChanged)
  public:
    explicit DesktopSession(QObject* parent = nullptr);
    ~DesktopSession() override;
    [[nodiscard]] bool initialize(HistoryController* history = nullptr);
    [[nodiscard]] QQuickWindow* window() const;
    [[nodiscard]] QQuickWindow* workspaceWindow() const;
    [[nodiscard]] QObject* styleTokens() const;
    [[nodiscard]] bool micaActive() const;
    [[nodiscard]] QColor systemAccent() const;
    Q_INVOKABLE void showWorkspace();
    Q_INVOKABLE void showSettings(bool confirmCapture = false);
    Q_INVOKABLE void copySelected(bool plainText = false);
    [[nodiscard]] bool trayAvailable() const;
    [[nodiscard]] unsigned long hotkeyError() const;
    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void replaySelected(bool copyOnly, bool plainText = false);
    [[nodiscard]] bool acrylicActive() const;
    [[nodiscard]] bool systemDark() const;
    [[nodiscard]] bool highContrast() const;
    [[nodiscard]] bool systemReducedMotion() const;
    [[nodiscard]] bool acrylicEnabled() const;
    void setAcrylicEnabled(bool enabled);
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
    void toggleWindow();
    void focusSearch();
    [[nodiscard]] const QString& shortcut() const;
    [[nodiscard]] const QString& shortcutStatus() const;
    Q_INVOKABLE bool setShortcut(const QString& shortcut);
  signals:
    void quitRequested();
    void shortcutChanged();
    void materialChanged();

  private slots:
    void updateMaterial();

  private:
    void openFromHotkey();
    void pasteRestoredText(quint32 sequence);
    void fitWindow(QQuickWindow* surface, bool center);
    bool mica_active_ = false;
    bool acrylic_active_ = false;
    bool acrylic_enabled_ = true;
    bool high_contrast_ = false;
    bool system_reduced_motion_ = false;
    bool material_update_pending_ = false;
    bool hotkey_open_pending_ = false;
    QPointer<HistoryController> history_;
    quintptr paste_target_ = 0;
    unsigned long paste_process_ = 0;
    unsigned long paste_thread_ = 0;
    quint64 paste_generation_ = 0;
    QQmlApplicationEngine engine_;
    QObject* style_tokens_ = nullptr;
    std::unique_ptr<QWK::QuickWindowAgent> window_agent_;
    std::unique_ptr<QWK::QuickWindowAgent> workspace_agent_;
    QPointer<QQuickWindow> replay_origin_;
    QQuickWindow* window_ = nullptr;
    QQuickWindow* workspace_window_ = nullptr;
    QAction show_action_;
    QAction quit_action_;
    QAction capture_action_;
    QMenu tray_menu_;
    QSystemTrayIcon tray_;
    GlobalHotkey hotkey_;
    QString shortcut_ = QStringLiteral("Win+Alt+V");
    QString shortcut_status_;
};
} // namespace reposco
