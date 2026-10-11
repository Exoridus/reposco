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
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchChanged)
    Q_PROPERTY(QString preview READ preview NOTIFY previewChanged)
    Q_PROPERTY(int retentionDays READ retentionDays NOTIFY stateChanged)
    Q_PROPERTY(bool available READ available NOTIFY stateChanged)
    Q_PROPERTY(bool captureActive READ captureActive NOTIFY stateChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(bool excludeSixDigitCodes READ excludeSixDigitCodes NOTIFY stateChanged)
    Q_PROPERTY(bool excludeEightDigitCodes READ excludeEightDigitCodes NOTIFY stateChanged)
    Q_PROPERTY(QStringList excludedApplications READ excludedApplications NOTIFY stateChanged)
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
    [[nodiscard]] bool excludeSixDigitCodes() const;
    [[nodiscard]] bool excludeEightDigitCodes() const;
    [[nodiscard]] const QStringList& excludedApplications() const;
    Q_INVOKABLE bool setCapturePolicy(bool sixDigits, bool eightDigits,
                                      const QStringList& applications);
    Q_INVOKABLE void clearHistory(bool confirmed);
    [[nodiscard]] const QString& searchQuery() const;
    [[nodiscard]] const QString& preview() const;
    void setSearchQuery(const QString& query);
    Q_INVOKABLE void selectEntry(qint64 id);
    Q_INVOKABLE void selectRow(int row);
    [[nodiscard]] int retentionDays() const;
    Q_INVOKABLE void setRetentionDays(int days, bool confirmed);
    void shutdown();
  signals:
    void stateChanged();
    void searchChanged();
    void previewChanged();
    void searchRequested(QString query, quint64 revision);
    void previewRequested(qint64 id, quint64 revision);
    void stopped();
    void recordRequested(QString text, quint64 token, quint64 generation);
    void preferenceRequested(bool active);
    void closeRequested();
    void policyRequested(bool sixDigits, bool eightDigits, QStringList applications);
    void clearRequested();
    void retentionRequested(int days);

  private:
    void applyActive(bool active);
    void refreshReady();
    void failClosed();
    void finishShutdown();
    void acceptCapture(const QString& text, quint64 token, quint64 generation);
    void requestSearch();
    void requestPreview();
    void resetPreview();
    QString search_query_;
    QString preview_;
    qint64 selected_id_ = 0;
    quint64 search_revision_ = 0;
    quint64 preview_revision_ = 0;
    bool search_pending_ = false;
    bool preview_pending_ = false;
    int retention_days_ = 90;
    bool six_digits_ = true;
    bool eight_digits_ = false;
    bool clearing_ = false;
    QStringList applications_;
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
