#include "platform/windows/KeyVault.h"
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
QTEST_GUILESS_MAIN(HistoryFlowTests)
#include "HistoryFlowTests.moc"
