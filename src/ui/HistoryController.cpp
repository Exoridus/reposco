#include "HistoryController.h"
#include "app/HistoryWorker.h"
#include <QDir>

namespace reposco {
HistoryController::HistoryController(QString directory, std::unique_ptr<ClipboardMonitor> monitor,
                                     QObject* parent)
    : QObject(parent), directory_(std::move(directory)), monitor_(std::move(monitor)),
      gate_(std::make_shared<CaptureGate>()) {
    if (!monitor_) {
        throw std::invalid_argument("A clipboard monitor is required");
    }
    qRegisterMetaType<HistorySummaries>();
    connect(monitor_.get(), &ClipboardMonitor::ready, this, [this] {
        monitor_ready_ = true;
        refreshReady();
    });
    connect(monitor_.get(), &ClipboardMonitor::captured, this, &HistoryController::acceptCapture);
    connect(monitor_.get(), &ClipboardMonitor::failure, this, &HistoryController::failClosed);
    connect(monitor_.get(), &ClipboardMonitor::stopped, this, [this] {
        monitor_stopped_ = true;
        if (!stopping_) {
            failClosed();
        }
        finishShutdown();
    });
    connect(monitor_.get(), &ClipboardMonitor::restored, this,
            [this](quint64 token, quint32 sequence, bool success) {
                if (token != replay_revision_ || !replay_busy_) {
                    return;
                }
                replay_busy_ = false;
                setReplayStatus(success ? tr("Copied text")
                                        : tr("Clipboard unavailable. Try again."));
                if (success) {
                    emit replayCompleted(sequence, replay_copy_only_);
                }
            });
    connect(&storage_thread_, &QThread::finished, this, [this] {
        storage_stopped_ = true;
        finishShutdown();
    });
}
HistoryController::~HistoryController() {
    shutdown();
    storage_thread_.wait();
}
void HistoryController::start() {
    if (started_ || stopping_) {
        return;
    }
    started_ = true;
    // The thread's finished connection schedules destruction on the worker's own thread.
    worker_ = new HistoryWorker(directory_, gate_);
    worker_->moveToThread(&storage_thread_);
    connect(&storage_thread_, &QThread::started, worker_, &HistoryWorker::initialize);
    connect(&storage_thread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(this, &HistoryController::recordRequested, worker_, &HistoryWorker::record);
    connect(this, &HistoryController::preferenceRequested, worker_, &HistoryWorker::setPreference);
    connect(this, &HistoryController::closeRequested, worker_, &HistoryWorker::close);
    connect(this, &HistoryController::policyRequested, worker_, &HistoryWorker::setPolicy);
    connect(this, &HistoryController::clearRequested, worker_, &HistoryWorker::clear);
    connect(this, &HistoryController::retentionRequested, worker_,
            &HistoryWorker::setRetentionDays);
    connect(this, &HistoryController::searchRequested, worker_, &HistoryWorker::search);
    connect(this, &HistoryController::previewRequested, worker_, &HistoryWorker::preview);
    connect(this, &HistoryController::replayRequested, worker_, &HistoryWorker::replay);
    connect(worker_, &HistoryWorker::replayReady, this,
            [this](const QString& text, quint64 revision) {
                replay_pending_ = false;
                if (revision != replay_revision_ || !replay_busy_) {
                    return;
                }
                if (!available_ || stopping_ || clearing_ || locked_ || text.isEmpty() ||
                    !monitor_->restoreText(text, revision)) {
                    replay_busy_ = false;
                    setReplayStatus(tr("Clipboard unavailable. Try again."));
                }
            });
    connect(worker_, &HistoryWorker::searchReady, this,
            [this](HistorySummaries rows, quint64 revision) {
                search_pending_ = false;
                if (!available_ || stopping_ || clearing_ || locked_) {
                    return;
                }
                if (revision == search_revision_) {
                    model_.replace(std::move(rows));
                } else {
                    requestSearch();
                }
            });
    connect(worker_, &HistoryWorker::previewReady, this,
            [this](const QString& text, quint64 revision) {
                preview_pending_ = false;
                if (!available_ || stopping_ || clearing_ || locked_) {
                    return;
                }
                if (revision == preview_revision_) {
                    preview_ = text;
                    emit previewChanged();
                } else {
                    requestPreview();
                }
            });
    connect(worker_, &HistoryWorker::policyApplied, this,
            [this](quint64 generation, bool sixDigits, bool eightDigits,
                   const QStringList& applications) {
                if (!policy_pending_ || generation != policy_generation_ || stopping_ || locked_) {
                    return;
                }
                six_digits_ = sixDigits;
                eight_digits_ = eightDigits;
                applications_ = applications;
                monitor_->setExcludedApplications(applications_);
                policy_pending_ = false;
                if (clearing_) {
                    emit stateChanged();
                } else {
                    applyActive(resume_after_policy_);
                }
            });
    connect(worker_, &HistoryWorker::cleared, this, [this] {
        clearing_ = false;
        if (!locked_ && !stopping_) {
            available_ = true;
            emit stateChanged();
        }
    });
    connect(worker_, &HistoryWorker::closed, &storage_thread_, &QThread::quit,
            Qt::DirectConnection);
    connect(worker_, &HistoryWorker::ready, this,
            [this](HistorySummaries rows, bool active, bool sixDigits, bool eightDigits,
                   const QStringList& applications, int retentionDays) {
                if (locked_ || stopping_) {
                    return;
                }
                model_.replace(std::move(rows));
                retention_days_ = retentionDays;
                six_digits_ = sixDigits;
                eight_digits_ = eightDigits;
                applications_ = applications;
                monitor_->setExcludedApplications(applications_);
                saved_active_ = active;
                storage_ready_ = true;
                refreshReady();
            });
    connect(worker_, &HistoryWorker::changed, this, [this](HistorySummaries rows) {
        if (!locked_ && !stopping_ && !clearing_) {
            if (search_query_.isEmpty()) {
                model_.replace(std::move(rows));
            } else {
                ++search_revision_;
                requestSearch();
            }
        }
    });
    connect(worker_, &HistoryWorker::processed, this, [this](quint64 token) {
        if (pending_token_ == token) {
            pending_token_ = 0;
        }
        monitor_->acknowledge(token);
    });
    connect(worker_, &HistoryWorker::locked, this, &HistoryController::failClosed);
    storage_thread_.start();
    monitor_->start();
}
QAbstractListModel* HistoryController::model() {
    return &model_;
}
bool HistoryController::available() const {
    return available_;
}
bool HistoryController::captureActive() const {
    return active_;
}
bool HistoryController::policyPending() const {
    return policy_pending_;
}
bool HistoryController::locked() const {
    return locked_;
}
const QString& HistoryController::status() const {
    return status_;
}
void HistoryController::setCaptureActive(bool active) {
    if (!available_ || stopping_ || clearing_ || policy_pending_) {
        return;
    }
    applyActive(active);
    emit preferenceRequested(active);
}
bool HistoryController::excludeSixDigitCodes() const {
    return six_digits_;
}
bool HistoryController::excludeEightDigitCodes() const {
    return eight_digits_;
}
const QStringList& HistoryController::excludedApplications() const {
    return applications_;
}
bool HistoryController::setCapturePolicy(bool sixDigits, bool eightDigits,
                                         const QStringList& applications) {
    if (!available_ || stopping_ || clearing_ || policy_pending_ || applications.size() > 32) {
        return false;
    }
    for (const auto& path : applications) {
        if (path.size() > 512 || !QDir::isAbsolutePath(path) ||
            !path.endsWith(".exe", Qt::CaseInsensitive)) {
            return false;
        }
    }
    resume_after_policy_ = active_;
    policy_pending_ = true;
    applyActive(false);
    policy_generation_ = generation_;
    status_ = tr("Applying privacy rules. Capture is paused until storage confirms.");
    emit stateChanged();
    emit policyRequested(sixDigits, eightDigits, applications, policy_generation_);
    return true;
}
void HistoryController::clearHistory(bool confirmed) {
    if (!confirmed || !available_ || stopping_ || clearing_) {
        return;
    }
    resume_after_policy_ = false;
    clearing_ = true;
    search_query_.clear();
    emit searchChanged();
    applyActive(false);
    available_ = false;
    resetPreview();
    ++search_revision_;
    model_.replace({});
    emit preferenceRequested(false);
    emit clearRequested();
    emit stateChanged();
}
void HistoryController::applyActive(bool active) {
    gate_->active.store(false);
    gate_->generation.store(++generation_);
    active_ = active;
    gate_->active.store(active);
    monitor_->setActive(active, generation_);
    status_ = active ? tr("Text capture is active. Marked secrets are excluded.")
                     : tr("Capture is paused. Existing clipboard contents are not collected.");
    emit stateChanged();
}
void HistoryController::refreshReady() {
    if (!available_ && !locked_ && !stopping_ && storage_ready_ && monitor_ready_) {
        available_ = true;
        applyActive(saved_active_);
        if (model_.rowCount() > 0) {
            selectRow(0);
        }
    }
}
void HistoryController::failClosed() {
    if (stopping_) {
        return;
    }
    applyActive(false);
    available_ = false;
    locked_ = true;
    policy_pending_ = false;
    resetPreview();
    ++search_revision_;
    model_.replace({});
    status_ = tr("History is locked or unavailable. Capture is disabled; files were preserved.");
    emit stateChanged();
}
void HistoryController::acceptCapture(const QString& text, quint64 token, quint64 generation) {
    if (token <= last_token_ || pending_token_ != 0 || !available_ || !active_ || stopping_ ||
        !gate_->permits(generation)) {
        monitor_->acknowledge(token);
        return;
    }
    last_token_ = token;
    pending_token_ = token;
    emit recordRequested(text, token, generation);
}
int HistoryController::retentionDays() const {
    return retention_days_;
}
void HistoryController::setRetentionDays(int days, bool confirmed) {
    if (!confirmed || !available_ || stopping_ || clearing_ || policy_pending_ || days < 1 ||
        days > 90) {
        return;
    }
    const bool resume = active_;
    applyActive(false);
    resetPreview();
    retention_days_ = days;
    emit retentionRequested(days);
    applyActive(resume);
}
const QString& HistoryController::searchQuery() const {
    return search_query_;
}
const QString& HistoryController::preview() const {
    return preview_;
}
void HistoryController::setSearchQuery(const QString& query) {
    const auto bounded = query.left(256);
    if (bounded == search_query_) {
        return;
    }
    search_query_ = bounded;
    ++search_revision_;
    resetPreview();
    emit searchChanged();
    requestSearch();
}
void HistoryController::requestSearch() {
    if (available_ && !search_pending_ && !stopping_ && !clearing_) {
        search_pending_ = true;
        emit searchRequested(search_query_, search_revision_);
    }
}
void HistoryController::selectRow(int row) {
    if (row < 0 || row >= model_.rowCount()) {
        resetPreview();
        return;
    }
    selectEntry(model_.data(model_.index(row, 0), HistoryModel::EntryIdRole).toLongLong());
}
void HistoryController::selectEntry(qint64 id) {
    if (!available_ || stopping_ || clearing_) {
        return;
    }
    resetPreview();
    selected_id_ = id;
    ++preview_revision_;
    preview_.clear();
    emit previewChanged();
    requestPreview();
}
void HistoryController::requestPreview() {
    if (available_ && selected_id_ > 0 && !preview_pending_ && !stopping_ && !clearing_) {
        preview_pending_ = true;
        emit previewRequested(selected_id_, preview_revision_);
    }
}
bool HistoryController::replayBusy() const {
    return replay_busy_;
}
const QString& HistoryController::replayStatus() const {
    return replay_status_;
}
void HistoryController::setReplayStatus(const QString& status) {
    replay_status_ = status;
    emit replayChanged();
}
void HistoryController::replaySelected(bool copyOnly) {
    if (!available_ || stopping_ || clearing_ || locked_ || selected_id_ <= 0 || replay_busy_ ||
        replay_pending_) {
        return;
    }
    replay_busy_ = true;
    replay_pending_ = true;
    replay_copy_only_ = copyOnly;
    setReplayStatus(tr("Restoring text..."));
    emit replayRequested(selected_id_, ++replay_revision_);
}
void HistoryController::cancelReplay() {
    ++replay_revision_;
    emit replayInvalidated();
    monitor_->cancelRestore();
    replay_busy_ = false;
    setReplayStatus({});
}
void HistoryController::resetPreview() {
    cancelReplay();
    selected_id_ = 0;
    ++preview_revision_;
    preview_.clear();
    emit previewChanged();
}
void HistoryController::shutdown() {
    resetPreview();
    if (stopping_) {
        return;
    }
    stopping_ = true;
    available_ = false;
    applyActive(false);
    if (!started_) {
        monitor_stopped_ = true;
        storage_stopped_ = true;
        finishShutdown();
        return;
    }
    emit closeRequested();
    monitor_->stop();
}
void HistoryController::finishShutdown() {
    if (stopping_ && monitor_stopped_ && storage_stopped_) {
        emit stopped();
    }
}
} // namespace reposco
