#include "core/TextContent.h"
#include "platform/windows/KeyVault.h"
#include "storage/HistoryStore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>
#include <array>

class HistoryTests : public QObject {
    Q_OBJECT
  private slots:
    void exactCodesAreExcludedBeforeIdentity() {
        for (const auto& value : {QString("123456"), QString(" 123456\r\n"),
                                  QString::fromUtf8("\xC2\xA0"
                                                    "123456"
                                                    "\xE2\x80\x83")}) {
            QVERIFY(!reposco::normalizeText(value));
        }
        for (const auto* value : {"12345", "1234567", "code 123456", "12345678"}) {
            QVERIFY(reposco::normalizeText(value));
        }
        QVERIFY(reposco::normalizeText(QString::fromUtf8("１２３４５６")));
    }
    void comparisonDoesNotChangeOriginal() {
        const auto text = reposco::normalizeText(QString::fromUtf8(" \r\ne\xCC\x81\rA \t"));
        QVERIFY(text);
        QCOMPARE(text->original, QByteArray(" \r\ne\xCC\x81\rA \t"));
        QCOMPARE(text->canonical, QByteArray("\xC3\xA9\nA"));
        QCOMPARE(text->fingerprint.size(), 32);
        QVERIFY(!reposco::normalizeText(QString::fromUtf8(" \xC2\xA0\n")));
        QVERIFY(!reposco::normalizeText(QString::fromUtf8(QByteArray("A\0B", 3))));
    }
    void protectedKeyPersistsAndFailsClosed() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QByteArray first;
        {
            const auto key = reposco::loadDatabaseKey(directory.path(), false);
            first = QByteArray(reinterpret_cast<const char*>(key->bytes().data()), 32);
            QVERIFY(first != QByteArray(32, '\0'));
        }
        {
            const auto key = reposco::loadDatabaseKey(directory.path(), true);
            QCOMPARE(QByteArray(reinterpret_cast<const char*>(key->bytes().data()), 32), first);
        }
        QFile envelope(directory.filePath("key.dat"));
        QVERIFY(envelope.open(QIODevice::ReadOnly));
        const auto saved = envelope.readAll();
        QVERIFY(saved.startsWith("RPKY"));
        QVERIFY(!saved.contains(first));
        envelope.close();
        QVERIFY(envelope.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(envelope.write("invalid"), 7);
        envelope.close();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 (void)reposco::loadDatabaseKey(directory.path(), true));
        QVERIFY(envelope.open(QIODevice::ReadOnly));
        QCOMPARE(envelope.readAll(), QByteArray("invalid"));
        envelope.close();
        QVERIFY(envelope.remove());
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 (void)reposco::loadDatabaseKey(directory.path(), true));
        QVERIFY(!envelope.exists());
    }
    void smartDedupIsAtomicAndKeepsNewestFormatting() {
        QTemporaryDir directory;
        const auto path = directory.filePath("history.db").toStdString();
        const std::array<unsigned char, 32> key = {0x31};
        reposco::HistoryStore store(path, key);
        store.record(*reposco::normalizeText("A"), 100);
        store.record(*reposco::normalizeText("B"), 101);
        store.record(*reposco::normalizeText("C"), 102);
        const auto before = store.summaries();
        QCOMPARE(before.size(), std::size_t{3});
        const auto firstId = before[2].id;
        store.record(*reposco::normalizeText(" \r\nA \t"), 103);
        const auto after = store.summaries();
        QCOMPARE(after.size(), std::size_t{3});
        QCOMPARE(after[0].id, firstId);
        QCOMPARE(after[0].excerpt, QString("A"));
        QCOMPARE(after[1].excerpt, QString("C"));
        QCOMPARE(after[2].excerpt, QString("B"));
        QCOMPARE(store.original(firstId), QByteArray(" \r\nA \t"));
        auto collision = *reposco::normalizeText("another value");
        collision.fingerprint = reposco::normalizeText("A")->fingerprint;
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, store.record(collision, 104));
        QCOMPARE(store.original(firstId), QByteArray(" \r\nA \t"));
        reposco::HistoryStore reopened(path, key);
        QCOMPARE(reopened.summaries().size(), std::size_t{3});
    }
    void budgetsAndSchemaFailClosed() {
        QTemporaryDir directory;
        const auto path = directory.filePath("history.db").toStdString();
        const std::array<unsigned char, 32> key = {0x31};
        {
            reposco::HistoryStore store(path, key, {2, 6, 1000});
            for (const auto* value : {"AA", "BB", "CC"}) {
                store.record(*reposco::normalizeText(value), 100);
            }
            QCOMPARE(store.summaries().size(), std::size_t{2});
            QCOMPARE(store.summaries()[0].excerpt, QString("CC"));
            store.prune(1101);
            QVERIFY(store.summaries().empty());
        }
        {
            reposco::EncryptedDatabase database(path, key);
            database.execute("PRAGMA user_version=99");
        }
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, reposco::HistoryStore(path, key));
    }
    void byteBudgetEvictsWithoutDependingOnCount() {
        QTemporaryDir directory;
        const std::array<unsigned char, 32> key = {0x31};
        reposco::HistoryStore store(directory.filePath("bytes.db").toStdString(), key,
                                    {10, 3, 1000});
        store.record(*reposco::normalizeText("AA"), 100);
        store.record(*reposco::normalizeText("BB"), 101);
        QCOMPARE(store.summaries().size(), std::size_t{1});
        QCOMPARE(store.summaries()[0].excerpt, QString("BB"));
    }
    void emptyExistingDatabaseIsNotReinitialized() {
        QTemporaryDir directory;
        const auto path = directory.filePath("history.db");
        const std::array<unsigned char, 32> key = {0x31};
        QFile truncated(path);
        QVERIFY(truncated.open(QIODevice::WriteOnly));
        truncated.close();
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 reposco::EncryptedDatabase(path.toStdString(), key));
        QCOMPARE(truncated.size(), 0);
    }
    void retiredEntryIdIsNeverReused() {
        QTemporaryDir directory;
        const std::array<unsigned char, 32> key = {0x31};
        reposco::HistoryStore store(directory.filePath("ids.db").toStdString(), key,
                                    {10, 100, 1000});
        store.record(*reposco::normalizeText("A"), 100);
        const auto retired = store.summaries()[0].id;
        store.prune(1101);
        QVERIFY(store.summaries().empty());
        store.record(*reposco::normalizeText("B"), 1102);
        QVERIFY(store.summaries()[0].id > retired);
    }
};

QTEST_GUILESS_MAIN(HistoryTests)
#include "HistoryTests.moc"
