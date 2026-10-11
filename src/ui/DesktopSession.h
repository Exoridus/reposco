#pragma once

#include "platform/windows/GlobalHotkey.h"

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QSystemTrayIcon>
#include <memory>

class QQuickWindow;
namespace QWK {
class QuickWindowAgent;
}

namespace reposco {
class HistoryController;
class DesktopSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString shortcut READ shortcut NOTIFY shortcutChanged)
    Q_PROPERTY(QString shortcutStatus READ shortcutStatus NOTIFY shortcutChanged)
  public:
    explicit DesktopSession(QObject* parent = nullptr);
    ~DesktopSession() override;
    [[nodiscard]] bool initialize(HistoryController* history = nullptr);
    [[nodiscard]] QQuickWindow* window() const;
    [[nodiscard]] bool trayAvailable() const;
    [[nodiscard]] unsigned long hotkeyError() const;
    Q_INVOKABLE void showWindow();
    void toggleWindow();
    void focusSearch();
    [[nodiscard]] const QString& shortcut() const;
    [[nodiscard]] const QString& shortcutStatus() const;
    Q_INVOKABLE bool setShortcut(const QString& shortcut);
  signals:
    void quitRequested();
    void shortcutChanged();

  private:
    QQmlApplicationEngine engine_;
    std::unique_ptr<QWK::QuickWindowAgent> window_agent_;
    QQuickWindow* window_ = nullptr;
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
