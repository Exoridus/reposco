#include "HistoryController.h"
#include "app/HistoryWorker.h"

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
    connect(worker_, &HistoryWorker::closed, &storage_thread_, &QThread::quit,
            Qt::DirectConnection);
    connect(worker_, &HistoryWorker::ready, this, [this](HistorySummaries rows, bool active) {
        if (locked_ || stopping_) {
            return;
        }
        model_.replace(std::move(rows));
        saved_active_ = active;
        storage_ready_ = true;
        refreshReady();
    });
    connect(worker_, &HistoryWorker::changed, this, [this](HistorySummaries rows) {
        if (!locked_ && !stopping_) {
            model_.replace(std::move(rows));
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
bool HistoryController::locked() const {
    return locked_;
}
const QString& HistoryController::status() const {
    return status_;
}
void HistoryController::setCaptureActive(bool active) {
    if (!available_ || stopping_) {
        return;
    }
    applyActive(active);
    emit preferenceRequested(active);
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
    }
}
void HistoryController::failClosed() {
    if (stopping_) {
        return;
    }
    applyActive(false);
    available_ = false;
    locked_ = true;
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
void HistoryController::shutdown() {
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
