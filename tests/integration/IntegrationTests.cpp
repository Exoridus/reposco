#include "platform/windows/Dpapi.h"
#include "platform/windows/GlobalHotkey.h"
#include "storage/EncryptedDatabase.h"

#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>
#include <sqlite3.h>

#include <array>
#include <memory>

class IntegrationTests : public QObject {
    Q_OBJECT
  private slots:
    void dpapiRoundTrip() {
        const QByteArray key(32, 'K');
        auto wrapped = reposco::protectKey(key);
        QVERIFY(wrapped.has_value());
        QVERIFY(*wrapped != key);
        auto unwrapped = reposco::unprotectKey(*wrapped);
        QVERIFY(unwrapped.has_value());
        QCOMPARE(*unwrapped, key);
        (*wrapped)[wrapped->size() / 2] ^= 0x5A;
        QVERIFY(!reposco::unprotectKey(*wrapped).has_value());
        QVERIFY(!reposco::protectKey({}).has_value());
        QVERIFY(!reposco::unprotectKey({}).has_value());
    }

    void sqlcipherEncryptedRoundTrip() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = (directory.path() + "/synthetic.db").toStdString();
        const std::array<unsigned char, 32> key = {0x42};
        {
            reposco::EncryptedDatabase database(path, key);
            QVERIFY(database.cipherVersion().starts_with("4.19.0"));
            QCOMPARE(database.cipherProvider(), std::string("openssl"));
            database.execute("CREATE TABLE proof(value TEXT NOT NULL)");
            database.execute("INSERT INTO proof VALUES('synthetic-only')");
            QCOMPARE(database.scalar("SELECT value FROM proof"), std::string("synthetic-only"));
            QCOMPARE(database.scalar("PRAGMA integrity_check"), std::string("ok"));
            QCOMPARE(database.scalar("PRAGMA cipher_integrity_check"), std::string{});
        }
        QFile file(QString::fromStdString(path));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(!file.readAll().contains("synthetic-only"));
        file.seek(0);
        QVERIFY(file.read(16) != QByteArray("SQLite format 3\0", 16));
        file.close();
        {
            reposco::EncryptedDatabase reopened(path, key);
            QCOMPARE(reopened.scalar("SELECT value FROM proof"), std::string("synthetic-only"));
        }
        const std::array<unsigned char, 32> wrongKey = {0x43};
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, reposco::EncryptedDatabase(path, wrongKey));
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, reposco::EncryptedDatabase(path, {}));

        sqlite3* raw = nullptr;
        QCOMPARE(sqlite3_open_v2(path.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr), SQLITE_OK);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> plaintext(raw, sqlite3_close);
        QCOMPARE(sqlite3_exec(plaintext.get(), "SELECT * FROM proof", nullptr, nullptr, nullptr),
                 SQLITE_NOTADB);
        QProcess ordinarySqlite;
        ordinarySqlite.start(
            REPOSCO_TEST_PYTHON,
            {"-c",
             "import sqlite3,sys\n"
             "try:\n"
             "    with sqlite3.connect(sys.argv[1]) as db:\n"
             "        db.execute('SELECT * FROM proof').fetchall()\n"
             "except sqlite3.DatabaseError as error:\n"
             "    sys.exit(7 if error.sqlite_errorcode == sqlite3.SQLITE_NOTADB else 8)\n",
             QString::fromStdString(path)});
        QVERIFY(ordinarySqlite.waitForFinished(10000));
        QCOMPARE(ordinarySqlite.exitStatus(), QProcess::NormalExit);
        QCOMPARE(ordinarySqlite.exitCode(), 7);
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,
                                 reposco::EncryptedDatabase(directory.path().toStdString(), key));
    }

    void hotkeyConflictAndCleanup() {
        reposco::GlobalHotkey first;
        const bool registered = first.registerDefault();
        if (!registered) {
            QVERIFY(first.errorCode() != 0);
            qInfo("Default hotkey unavailable; native conflict reported");
            return;
        }
        reposco::GlobalHotkey second;
        QVERIFY(!second.registerDefault());
        QVERIFY(second.errorCode() != 0);
        first.unregister();
        QVERIFY(second.registerDefault());
    }
};

QTEST_GUILESS_MAIN(IntegrationTests)
#include "IntegrationTests.moc"
