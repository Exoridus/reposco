#include "HistoryWorker.h"

#include "platform/windows/KeyVault.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace reposco {
HistoryWorker::HistoryWorker(QString directory, std::shared_ptr<CaptureGate> gate)
    : directory_(std::move(directory)), gate_(std::move(gate)) {
}
void HistoryWorker::initialize() {
    try {
        if (directory_.isEmpty() || QFileInfo(directory_).isSymLink() ||
            !QDir().mkpath(directory_)) {
            throw std::runtime_error("Private history directory is unavailable");
        }
        lock_ = std::make_unique<QLockFile>(directory_ + "/history.lock");
        lock_->setStaleLockTime(0);
        if (!lock_->tryLock(0)) {
            throw std::runtime_error("History is already in use");
        }
        const auto databasePath = directory_ + "/history.db";
        const bool historyExists = QFileInfo::exists(databasePath);
        if (!historyExists && QFileInfo::exists(directory_ + "/key.dat")) {
            throw std::runtime_error("History is missing beside an existing protected key");
        }
        const auto key = loadDatabaseKey(directory_, historyExists);
        store_ = std::make_unique<HistoryStore>(databasePath.toStdString(), key->bytes());
        store_->prune(QDateTime::currentMSecsSinceEpoch());
        preferences_ =
            std::make_unique<QSettings>(directory_ + "/settings.ini", QSettings::IniFormat);
        const bool captureEnabled = preferences_->value("captureEnabled", false).toBool();
        if (preferences_->status() != QSettings::NoError) {
            throw std::runtime_error("Capture preference is unreadable");
        }
        maintenance_ = new QTimer(this);
        maintenance_->setInterval(60000);
        connect(maintenance_, &QTimer::timeout, this, [this] {
            if (!store_) {
                return;
            }
            try {
                store_->prune(QDateTime::currentMSecsSinceEpoch());
                emit changed(store_->summaries());
            } catch (const std::exception&) {
                failClosed();
            }
        });
        maintenance_->start();
        six_digits_ = preferences_->value("excludeSixDigitCodes", true).toBool();
        eight_digits_ = preferences_->value("excludeEightDigitCodes", false).toBool();
        const auto applications = preferences_->value("excludedApplications").toStringList();
        if (applications.size() > 32) {
            throw std::runtime_error("Application exclusions exceed the supported budget");
        }
        for (const auto& path : applications) {
            if (path.size() > 512 || !QDir::isAbsolutePath(path) ||
                !path.endsWith(".exe", Qt::CaseInsensitive)) {
                throw std::runtime_error("Application exclusion is invalid");
            }
        }
        emit ready(store_->summaries(), captureEnabled, six_digits_, eight_digits_, applications);
    } catch (const std::exception&) {
        failClosed();
    }
}
void HistoryWorker::record(const QString& text, quint64 token, quint64 generation) {
    try {
        if (store_ && gate_->permits(generation)) {
            const auto normalized = normalizeText(text, six_digits_, eight_digits_);
            if (normalized && gate_->permits(generation)) {
                store_->record(*normalized, QDateTime::currentMSecsSinceEpoch());
                emit changed(store_->summaries());
            }
        }
    } catch (const std::exception&) {
        failClosed();
    }
    emit processed(token);
}
void HistoryWorker::setPreference(bool active) {
    if (!preferences_ || !store_) {
        return;
    }
    preferences_->setValue("captureEnabled", active);
    preferences_->sync();
    if (preferences_->status() != QSettings::NoError) {
        failClosed();
    }
}
void HistoryWorker::setPolicy(bool sixDigits, bool eightDigits, const QStringList& applications) {
    if (!preferences_ || !store_) {
        return;
    }
    six_digits_ = sixDigits;
    eight_digits_ = eightDigits;
    preferences_->setValue("excludeSixDigitCodes", sixDigits);
    preferences_->setValue("excludeEightDigitCodes", eightDigits);
    preferences_->setValue("excludedApplications", applications);
    preferences_->sync();
    if (preferences_->status() != QSettings::NoError) {
        failClosed();
    }
}
void HistoryWorker::clear() {
    try {
        if (store_) {
            store_->clear();
            emit changed({});
            emit cleared();
        }
    } catch (const std::exception&) {
        failClosed();
    }
}
void HistoryWorker::failClosed() {
    gate_->active.store(false);
    if (maintenance_) {
        maintenance_->stop();
    }
    store_.reset();
    emit locked();
}
void HistoryWorker::close() {
    gate_->active.store(false);
    if (maintenance_) {
        maintenance_->stop();
    }
    store_.reset();
    preferences_.reset();
    lock_.reset();
    emit closed();
}
} // namespace reposco
