#include "app/HistoryWorker.h"
#include "platform/windows/KeyVault.h"
#include "platform/windows/NativeClipboardMonitor.h"
#include "ui/HistoryController.h"
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace {
class SyntheticMonitor final : public reposco::ClipboardMonitor {
  public:
    bool active = false;
    quint64 generation = 0;
    int acknowledgements = 0;
    bool supportsRestore = false;
    int writes = 0;
    QString restoredText;
    reposco::ClipboardContent restoredContent;
    bool restoreContent(const reposco::ClipboardContent& content, quint64 token) override {
        restoredContent = content;
        return restoreText(content.text, token);
    }
    bool restoreText(const QString& text, quint64 token) override {
        if (!supportsRestore) {
            return false;
        }
        ++writes;
        restoredText = text;
        emit restored(token, 42, true);
        return true;
    }
    void start() override {
        emit ready();
    }
    void setActive(bool enabled, quint64 epoch) override {
        active = enabled;
        generation = epoch;
    }
    void acknowledge(quint64) override {
        ++acknowledgements;
    }
    void stop() override {
        emit stopped();
    }
};
} // namespace

class HistoryFlowTests : public QObject {
    Q_OBJECT
  private slots:
    void richCaptureSearchAndReplayKeepOriginalRepresentations() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        source->supportsRestore = true;
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        reposco::ClipboardContent raw;
        raw.text = "Synthetic formatted text";
        raw.rtf = "{\\rtf1\\ansi Synthetic formatted text}";
        emit source->capturedContent(raw, 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(controller.model()->rowCount(), 1, 5000);
        controller.setSearchQuery("formatted");
        QTRY_COMPARE_WITH_TIMEOUT(controller.model()->rowCount(), 1, 5000);
        controller.selectRow(0);
        QTRY_COMPARE_WITH_TIMEOUT(controller.preview(), raw.text, 5000);
        controller.replaySelected(true);
        QTRY_COMPARE_WITH_TIMEOUT(source->writes, 1, 5000);
        QCOMPARE(source->restoredContent.rtf, raw.rtf);
        QCOMPARE(source->restoredText, raw.text);
        controller.replaySelected(true, true);
        QTRY_COMPARE_WITH_TIMEOUT(source->writes, 2, 5000);
        QVERIFY(source->restoredContent.rtf.isEmpty());
        QCOMPARE(source->restoredText, raw.text);
        QCOMPARE(controller.model()->rowCount(), 1);
        controller.shutdown();
    }
    void clearDuringPolicyChangeKeepsNewRulesAndStaysPaused() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        QVERIFY(controller.setCapturePolicy(false, true, {}));
        controller.clearHistory(true);
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        QVERIFY(!controller.captureActive());
        QVERIFY(!controller.excludeSixDigitCodes());
        QVERIFY(controller.excludeEightDigitCodes());
        controller.shutdown();
    }
    void policyIsNotEffectiveUntilStorageAcknowledges() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        QVERIFY(controller.setCapturePolicy(false, true, {}));
        QVERIFY(!controller.captureActive());
        QVERIFY(!source->active);
        QVERIFY(controller.excludeSixDigitCodes());
        QVERIFY(!controller.setCapturePolicy(true, false, {}));
        controller.setCaptureActive(true);
        QVERIFY(!controller.captureActive());
        QTRY_VERIFY_WITH_TIMEOUT(controller.captureActive(), 5000);
        QVERIFY(!controller.excludeSixDigitCodes());
        QVERIFY(controller.excludeEightDigitCodes());
        emit source->captured("12345678", 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, 1, 5000);
        QCOMPARE(controller.model()->rowCount(), 0);
        controller.shutdown();
    }
    void replayUsesFullOriginalAndClearCancelsPendingReply() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        source->supportsRestore = true;
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        const auto original = QString("Original line\r\n") + QString(10000, 'x') + "  ";
        emit source->captured(original, 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(controller.model()->rowCount(), 1, 5000);
        controller.selectRow(0);
        QTRY_COMPARE_WITH_TIMEOUT(controller.preview().size(), 8192, 5000);
        QCOMPARE(source->writes, 0);
        QSignalSpy completed(&controller, &reposco::HistoryController::replayCompleted);
        controller.replaySelected(true);
        QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
        QVERIFY(source->restoredText == original);
        QCOMPARE(completed[0][1].toBool(), true);
        QCOMPARE(controller.model()->rowCount(), 1);
        controller.replaySelected(false);
        controller.clearHistory(true);
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        QCOMPARE(controller.model()->rowCount(), 0);
        QCOMPARE(source->writes, 1);
        QCOMPARE(completed.count(), 1);
        QVERIFY(!controller.replayBusy());
        controller.shutdown();
    }
    void canceledReplayKeepsTheStorageQueueBounded() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        source->supportsRestore = true;
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        emit source->captured("synthetic bounded replay", 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(controller.model()->rowCount(), 1, 5000);
        controller.selectRow(0);
        QSignalSpy requested(&controller, &reposco::HistoryController::replayRequested);
        controller.replaySelected(true);
        controller.cancelReplay();
        controller.replaySelected(true);
        QCOMPARE(requested.count(), 1);
        QTest::qWait(100);
        QCOMPARE(source->writes, 0);
        controller.replaySelected(true);
        QTRY_COMPARE_WITH_TIMEOUT(source->writes, 1, 5000);
        controller.shutdown();
    }
    void replayIsExplicitAndUnsupportedWriterFailsSafely() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        emit source->captured("synthetic replay original", 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(controller.model()->rowCount(), 1, 5000);
        controller.selectRow(0);
        QVERIFY(QMetaObject::invokeMethod(&controller, "replaySelected", Q_ARG(bool, true)));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.property("replayBusy").toBool(), 5000);
        QVERIFY(controller.property("replayStatus").toString().contains("unavailable"));
        QCOMPARE(controller.model()->rowCount(), 1);
        controller.shutdown();
    }
    void policyChangeDuringCaptureRejectsOldGeneration() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        const auto obsoleteGeneration = source->generation;
        QVERIFY(controller.setCapturePolicy(true, true, {"C:/Synthetic/Vault.exe"}));
        QTRY_VERIFY_WITH_TIMEOUT(controller.captureActive(), 5000);
        QVERIFY(source->generation != obsoleteGeneration);
        emit source->captured("obsolete policy event", 1, obsoleteGeneration);
        QCOMPARE(source->acknowledgements, 1);
        QCOMPARE(controller.model()->rowCount(), 0);
        emit source->captured("123456", 2, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, 2, 5000);
        QCOMPARE(controller.model()->rowCount(), 0);
        emit source->captured("12345678", 3, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, 3, 5000);
        QCOMPARE(controller.model()->rowCount(), 0);
        emit source->captured("current policy event", 4, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, 4, 5000);
        QCOMPARE(controller.model()->rowCount(), 1);
        QSignalSpy stopped(&controller, &reposco::HistoryController::stopped);
        controller.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void queuedRecordCannotUseAnObsoletePolicyGeneration() {
        QTemporaryDir directory;
        auto gate = std::make_shared<reposco::CaptureGate>();
        reposco::HistoryWorker worker(directory.path(), gate);
        QSignalSpy ready(&worker, &reposco::HistoryWorker::ready);
        QSignalSpy changed(&worker, &reposco::HistoryWorker::changed);
        QSignalSpy processed(&worker, &reposco::HistoryWorker::processed);
        worker.initialize();
        QCOMPARE(ready.count(), 1);
        worker.setPolicy(false, false, {});
        gate->generation.store(1);
        gate->active.store(true);
        QVERIFY(QMetaObject::invokeMethod(&worker, "record", Qt::QueuedConnection,
                                          Q_ARG(QString, QString("obsolete queued event")),
                                          Q_ARG(quint64, 1), Q_ARG(quint64, 1)));
        gate->active.store(false);
        gate->generation.store(2);
        worker.setPolicy(true, false, {});
        gate->active.store(true);
        QTRY_COMPARE_WITH_TIMEOUT(processed.count(), 1, 5000);
        QCOMPARE(changed.count(), 0);
        worker.close();
        const auto key = reposco::loadDatabaseKey(directory.path(), true);
        reposco::HistoryStore stored(directory.filePath("history.db").toStdString(), key->bytes());
        QVERIFY(stored.summaries().empty());
    }
    void exactCodePolicyPersistsAndClearInvalidatesQueuedCapture() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        QVERIFY(controller.excludeSixDigitCodes());
        controller.setRetentionDays(7, false);
        QCOMPARE(controller.retentionDays(), 90);
        controller.setRetentionDays(7, true);
        QCOMPARE(controller.retentionDays(), 7);
        QVERIFY(!controller.excludeEightDigitCodes());
        controller.setCaptureActive(true);
        quint64 token = 1;
        for (const auto* value : {"123456", " 123456\r\n", "12345", "1234567", "code 123456"}) {
            emit source->captured(value, token, source->generation);
            QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, static_cast<int>(token), 5000);
            ++token;
        }
        QCOMPARE(controller.model()->rowCount(), 3);
        {
            const auto key = reposco::loadDatabaseKey(directory.path(), true);
            reposco::HistoryStore stored(directory.filePath("history.db").toStdString(),
                                         key->bytes());
            const auto rows = stored.summaries();
            QCOMPARE(rows.size(), std::size_t{3});
            QCOMPARE(rows[0].excerpt, QString("code 123456"));
            QCOMPARE(rows[1].excerpt, QString("1234567"));
            QCOMPARE(rows[2].excerpt, QString("12345"));
        }
        QVERIFY(controller.setCapturePolicy(false, true, {"C:/Synthetic/Vault.exe"}));
        QTRY_VERIFY_WITH_TIMEOUT(controller.captureActive(), 5000);
        emit source->captured("123456", token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, static_cast<int>(token), 5000);
        QCOMPARE(controller.model()->rowCount(), 4);
        ++token;
        emit source->captured("12345678", token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, static_cast<int>(token), 5000);
        QCOMPARE(controller.model()->rowCount(), 4);
        controller.clearHistory(false);
        QCOMPARE(controller.model()->rowCount(), 4);
        const auto oldGeneration = source->generation;
        emit source->captured("queued synthetic", ++token, oldGeneration);
        controller.clearHistory(true);
        QCOMPARE(controller.model()->rowCount(), 0);
        QVERIFY(!controller.captureActive());
        emit source->captured("stale synthetic", ++token, oldGeneration);
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        QCOMPARE(controller.model()->rowCount(), 0);
        QSignalSpy stopped(&controller, &reposco::HistoryController::stopped);
        controller.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
        const auto key = reposco::loadDatabaseKey(directory.path(), true);
        {
            reposco::HistoryStore store(directory.filePath("history.db").toStdString(),
                                        key->bytes());
            QVERIFY(store.summaries().empty());
        }
        QVERIFY(!QFile::exists(directory.filePath("history.db-wal")));
        reposco::HistoryController reopened(directory.path(), std::make_unique<SyntheticMonitor>());
        reopened.start();
        QTRY_VERIFY_WITH_TIMEOUT(reopened.available(), 5000);
        QVERIFY(!reopened.captureActive());
        QCOMPARE(reopened.retentionDays(), 7);
        QVERIFY(!reopened.excludeSixDigitCodes());
        QVERIFY(reopened.excludeEightDigitCodes());
        QCOMPARE(reopened.excludedApplications(), QStringList{"C:/Synthetic/Vault.exe"});
        QVERIFY(!reopened.setCapturePolicy(true, false, {"Vault.exe"}));
        QSignalSpy reopenedStopped(&reopened, &reposco::HistoryController::stopped);
        reopened.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(reopenedStopped.count(), 1, 5000);
    }
    void applicationRulesMatchExactPathsAndFailClosedForUnknownOwners() {
        const QStringList rules{"C:/Synthetic/Vault.exe"};
        QVERIFY(reposco::isApplicationExcluded("c:\\synthetic\\VAULT.EXE", rules));
        QVERIFY(reposco::isApplicationExcluded({}, rules));
        QVERIFY(!reposco::isApplicationExcluded("C:/Elsewhere/Vault.exe", rules));
        QVERIFY(!reposco::isApplicationExcluded("C:/Synthetic/Browser.exe", rules));
        QVERIFY(!reposco::isApplicationExcluded({}, {}));
    }
    void searchUsesStoredTextAndPreviewIsBoundedOriginal() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        controller.setCaptureActive(true);
        emit source->captured("  Architecture\r\nnotes  ", 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, 1, 5000);
        emit source->captured("Other synthetic", 2, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, 2, 5000);
        QVERIFY(controller.setProperty("searchQuery", "architecture"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.model()->rowCount(), 1, 5000);
        const auto id =
            controller.model()
                ->data(controller.model()->index(0, 0), reposco::HistoryModel::EntryIdRole)
                .toLongLong();
        QVERIFY(QMetaObject::invokeMethod(&controller, "selectEntry", Q_ARG(qint64, id)));
        QTRY_COMPARE_WITH_TIMEOUT(controller.property("preview").toString(),
                                  QString("  Architecture\r\nnotes  "), 5000);
        QVERIFY(controller.setProperty("searchQuery", "no synthetic match"));
        QTRY_COMPARE_WITH_TIMEOUT(controller.model()->rowCount(), 0, 5000);
        controller.clearHistory(true);
        QVERIFY(controller.property("preview").toString().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        QSignalSpy stopped(&controller, &reposco::HistoryController::stopped);
        controller.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void optInDedupPauseAndShutdown() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        QSignalSpy stopped(&controller, &reposco::HistoryController::stopped);
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.available(), 5000);
        QVERIFY(!source->active);
        QVERIFY(!controller.captureActive());
        emit source->captured("not consented", 1, source->generation);
        QCOMPARE(controller.model()->rowCount(), 0);
        controller.setCaptureActive(true);
        QVERIFY(source->active);
        for (int index = 0; index < 4; ++index) {
            const QString text = index == 3 ? " A " : QString(QChar('A' + index));
            emit source->captured(text, static_cast<quint64>(index + 2), source->generation);
            QTRY_COMPARE_WITH_TIMEOUT(source->acknowledgements, index + 2, 5000);
        }
        QCOMPARE(controller.model()->rowCount(), 3);
        QCOMPARE(controller.model()
                     ->data(controller.model()->index(0, 0), reposco::HistoryModel::ExcerptRole)
                     .toString(),
                 QString("A"));
        const auto previousGeneration = source->generation;
        controller.setCaptureActive(false);
        QVERIFY(!source->active);
        emit source->captured("stale secret", 6, previousGeneration);
        QCOMPARE(controller.model()->rowCount(), 3);
        controller.setCaptureActive(true);
        emit source->captured("stale resumed", 7, previousGeneration);
        QCOMPARE(controller.model()->rowCount(), 3);
        controller.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
        {
            const auto key = reposco::loadDatabaseKey(directory.path(), true);
            reposco::HistoryStore store(directory.filePath("history.db").toStdString(),
                                        key->bytes());
            QCOMPARE(store.summaries().size(), std::size_t{3});
        }
        QFile file(directory.filePath("history.db"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(16) != QByteArray("SQLite format 3\0", 16));
    }
    void persistedConsentAndExclusiveWriter() {
        QTemporaryDir directory;
        {
            reposco::HistoryController first(directory.path(),
                                             std::make_unique<SyntheticMonitor>());
            first.start();
            QTRY_VERIFY_WITH_TIMEOUT(first.available(), 5000);
            first.setCaptureActive(true);
            reposco::HistoryController second(directory.path(),
                                              std::make_unique<SyntheticMonitor>());
            second.start();
            QTRY_VERIFY_WITH_TIMEOUT(second.locked(), 5000);
            QSignalSpy secondStopped(&second, &reposco::HistoryController::stopped);
            second.shutdown();
            QTRY_COMPARE_WITH_TIMEOUT(secondStopped.count(), 1, 5000);
            QSignalSpy firstStopped(&first, &reposco::HistoryController::stopped);
            first.shutdown();
            QTRY_COMPARE_WITH_TIMEOUT(firstStopped.count(), 1, 5000);
        }
        reposco::HistoryController reopened(directory.path(), std::make_unique<SyntheticMonitor>());
        reopened.start();
        QTRY_VERIFY_WITH_TIMEOUT(reopened.available(), 5000);
        QVERIFY(reopened.captureActive());
        reopened.setCaptureActive(false);
        QSignalSpy stopped(&reopened, &reposco::HistoryController::stopped);
        reopened.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void missingKeyLocksExistingHistoryWithoutReplacement() {
        QTemporaryDir directory;
        QFile history(directory.filePath("history.db"));
        QVERIFY(history.open(QIODevice::WriteOnly));
        QCOMPARE(history.write("original-history"), 16);
        history.close();
        auto monitor = std::make_unique<SyntheticMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController controller(directory.path(), std::move(monitor));
        controller.start();
        QTRY_VERIFY_WITH_TIMEOUT(controller.locked(), 5000);
        controller.setCaptureActive(true);
        QVERIFY(!source->active);
        QVERIFY(!QFile::exists(directory.filePath("key.dat")));
        QVERIFY(history.open(QIODevice::ReadOnly));
        QCOMPARE(history.readAll(), QByteArray("original-history"));
        QSignalSpy stopped(&controller, &reposco::HistoryController::stopped);
        controller.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
};
QTEST_MAIN(HistoryFlowTests)
#include "HistoryFlowTests.moc"
