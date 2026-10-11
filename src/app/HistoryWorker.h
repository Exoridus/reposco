#pragma once

#include "storage/HistoryStore.h"
#include <QLockFile>
#include <QObject>
#include <QSettings>
#include <QTimer>
#include <atomic>
#include <memory>

namespace reposco {
using HistorySummaries = std::vector<HistorySummary>;
struct CaptureGate {
    std::atomic<bool> active{false};
    std::atomic<quint64> generation{0};
    [[nodiscard]] bool permits(quint64 epoch) const {
        return active.load() && generation.load() == epoch;
    }
};

class HistoryWorker final : public QObject {
    Q_OBJECT
  public:
    HistoryWorker(QString directory, std::shared_ptr<CaptureGate> gate);
  public slots:
    void initialize();
    void record(const QString& text, quint64 token, quint64 generation);
    void setPreference(bool active);
    void setPolicy(bool sixDigits, bool eightDigits, const QStringList& applications);
    void clear();
    void close();
  signals:
    void ready(reposco::HistorySummaries rows, bool captureEnabled, bool sixDigits,
               bool eightDigits, QStringList applications);
    void changed(reposco::HistorySummaries rows);
    void processed(quint64 token);
    void locked();
    void closed();
    void cleared();

  private:
    void failClosed();
    QString directory_;
    std::shared_ptr<CaptureGate> gate_;
    std::unique_ptr<QLockFile> lock_;
    std::unique_ptr<QSettings> preferences_;
    std::unique_ptr<HistoryStore> store_;
    QTimer* maintenance_ = nullptr;
    bool six_digits_ = true;
    bool eight_digits_ = false;
};
} // namespace reposco
Q_DECLARE_METATYPE(reposco::HistorySummaries)
