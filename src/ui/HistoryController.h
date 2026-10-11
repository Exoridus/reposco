#pragma once

#include "HistoryModel.h"
#include "platform/interfaces/ClipboardMonitor.h"
#include <QPointer>
#include <QThread>
#include <QtQml/qqmlregistration.h>
#include <memory>

namespace reposco {
class HistoryWorker;
struct CaptureGate;
class HistoryController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by the application")
    Q_PROPERTY(QAbstractListModel* model READ model CONSTANT)
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool captureActive READ captureActive NOTIFY stateChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
  public:
    HistoryController(QString directory, std::unique_ptr<ClipboardMonitor> monitor,
                      QObject* parent = nullptr);
    ~HistoryController() override;
    void start();
    [[nodiscard]] QAbstractListModel* model();
    [[nodiscard]] bool available() const;
    [[nodiscard]] bool captureActive() const;
    [[nodiscard]] bool locked() const;
    [[nodiscard]] const QString& status() const;
    Q_INVOKABLE void setCaptureActive(bool active);
    void shutdown();
  signals:
    void stateChanged();
    void stopped();
    void recordRequested(QString text, quint64 token, quint64 generation);
    void preferenceRequested(bool active);
    void closeRequested();

  private:
    void applyActive(bool active);
    void refreshReady();
    void failClosed();
    void finishShutdown();
    void acceptCapture(const QString& text, quint64 token, quint64 generation);
    QString directory_;
    std::unique_ptr<ClipboardMonitor> monitor_;
    HistoryModel model_;
    QThread storage_thread_;
    QPointer<HistoryWorker> worker_;
    std::shared_ptr<CaptureGate> gate_;
    bool started_ = false;
    bool available_ = false;
    bool active_ = false;
    bool locked_ = false;
    bool storage_ready_ = false;
    bool monitor_ready_ = false;
    bool saved_active_ = false;
    bool stopping_ = false;
    bool monitor_stopped_ = false;
    bool storage_stopped_ = false;
    quint64 generation_ = 0;
    quint64 last_token_ = 0;
    quint64 pending_token_ = 0;
    QString status_ = QStringLiteral("Opening encrypted history...");
};
} // namespace reposco
