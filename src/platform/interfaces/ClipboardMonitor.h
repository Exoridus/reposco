#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace reposco {
class ClipboardMonitor : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    virtual void start() = 0;
    // Activation never reads the clipboard that was present before the new generation.
    virtual void setActive(bool active, quint64 generation) = 0;
    virtual void acknowledge(quint64 token) = 0;
    virtual void setExcludedApplications(const QStringList&) {
    }
    // Restores at most one explicit request; implementations may decline unsupported writes.
    virtual bool restoreText(const QString&, quint64) {
        return false;
    }
    virtual void cancelRestore() {
    }
    virtual void stop() = 0;
  signals:
    void ready();
    void restored(quint64 token, quint32 sequence, bool success);
    void captured(QString text, quint64 token, quint64 generation);
    void failure(QString message);
    void stopped();
};
} // namespace reposco
