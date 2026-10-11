#include "ClipboardFixtures.h"
#include "content/ClipboardContent.h"
#include "content/ContentPreview.h"
#include "storage/HistoryStore.h"
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>
#include <QtEndian>
#include <QtTest>
#include <array>

using reposco::testing::htmlClipboard;
class ContentTests : public QObject {
    Q_OBJECT
  private slots:
    void htmlImportRejectsDiagnosticValuesAndQuotedSpanBypasses() {
        QTest::failOnWarning();
        reposco::ClipboardContent raw;
        raw.html = htmlClipboard("<font color='SYNTHETIC_SECRET'>synthetic</font>");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html =
            htmlClipboard("<table><tr><td title='>' colspan='20480'>synthetic</td></tr></table>");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard("<font face='Arial'color='SYNTHETIC_SECRET'>synthetic</font>");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard(
            "<table><tr><td colspan='+20480' title='colspan=1 '>synthetic</td></tr></table>");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard("</x a='><font color=SYNTHETIC_SECRET> '>synthetic");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard("<!x a='><font color=SYNTHETIC_SECRET> '>synthetic");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard(
            R"(<font title='x\' ignored=' color=SYNTHETIC_SECRET '>synthetic</font>)");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard("<table><tr><td bgcolor='#336699'>synthetic</td></tr></table>");
        QVERIFY(reposco::normalizeClipboard(raw));
    }
    void bitmapPayloadIsBoundedAndKeepsPixels() {
        QImage image(3, 2, QImage::Format_RGB32);
        image.fill(Qt::green);
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "BMP"));
        reposco::ClipboardContent raw;
        raw.dib = buffer.data().sliced(14);
        const auto original = raw.dib;
        raw.dib += "synthetic allocation padding";
        const auto content = reposco::normalizeClipboard(raw);
        QVERIFY(content);
        const auto decoded = reposco::decodeFormats(content->formats, content->text.original);
        QVERIFY(decoded);
        QCOMPARE(decoded->dib, original);
        QCOMPARE(reposco::clipboardImage(*decoded).size(), image.size());
        raw.dib.chop(raw.dib.size() - 39);
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.dib = original;
        qToLittleEndian<qint32>(INT_MAX, raw.dib.data() + 4);
        QVERIFY(!reposco::normalizeClipboard(raw));
    }
    void allocationPaddingAndExcessiveMarkupAreRejectedOrRemoved() {
        QImage image(2, 2, QImage::Format_ARGB32);
        image.fill(Qt::blue);
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        reposco::ClipboardContent raw;
        raw.png = buffer.data() + "synthetic unused allocation";
        const auto normalized = reposco::normalizeClipboard(raw);
        QVERIFY(normalized);
        const auto decoded = reposco::decodeFormats(normalized->formats, normalized->text.original);
        QVERIFY(decoded);
        QCOMPARE(decoded->png, buffer.data());
        raw = {};
        raw.html = htmlClipboard(QByteArray("<div>").repeated(100) + "synthetic" +
                                 QByteArray("</div>").repeated(100));
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard("<table><tr><td colspan='17'>synthetic</td></tr></table>");
        QVERIFY(!reposco::normalizeClipboard(raw));
    }
    void encryptedV1MigrationPreservesTextAndStoresRichFormats() {
        QTemporaryDir directory;
        const auto path = directory.filePath("history.db").toStdString();
        const std::array<unsigned char, 32> key = {0x31};
        const auto oldText = reposco::normalizeText("A");
        QVERIFY(oldText);
        {
            reposco::EncryptedDatabase database(path, key);
            database.execute("CREATE TABLE schema_info(version INTEGER NOT NULL, "
                             "canonicalization_version INTEGER NOT NULL)");
            database.execute("INSERT INTO schema_info VALUES(1,1)");
            database.execute(
                "CREATE TABLE history(id INTEGER PRIMARY KEY AUTOINCREMENT, fingerprint BLOB NOT "
                "NULL UNIQUE, canonical TEXT NOT NULL, original BLOB NOT NULL, first_seen_ms "
                "INTEGER NOT NULL, last_copied_ms INTEGER NOT NULL, ordinal INTEGER NOT NULL "
                "UNIQUE, payload_bytes INTEGER NOT NULL)");
            database.execute("CREATE INDEX history_retention ON history(last_copied_ms)");
            database.execute("INSERT INTO history VALUES(7,X'" +
                             oldText->fingerprint.toHex().toStdString() +
                             "','A',X'41',100,100,1,1)");
            database.execute("PRAGMA user_version=1");
        }
        {
            reposco::HistoryStore store(path, key);
            QCOMPARE(store.summaries().front().id, std::int64_t{7});
            QCOMPARE(store.original(7), QByteArray("A"));
            reposco::ClipboardContent raw;
            raw.text = "Synthetic bold";
            raw.html = htmlClipboard("<b>Synthetic bold</b>");
            const auto rich = reposco::normalizeClipboard(raw);
            QVERIFY(rich);
            QVERIFY(store.record(*rich, 101, [] { return true; }));
            const auto stored = store.content(store.summaries().front().id);
            QVERIFY(stored);
            QCOMPARE(stored->html, raw.html);
            QCOMPARE(stored->text, raw.text);
            QCOMPARE(store.summaries("bold").size(), std::size_t{1});
            store.record(*oldText, 102);
            QCOMPARE(store.summaries().front().id, std::int64_t{7});
            QCOMPARE(store.summaries().size(), std::size_t{2});
            store.clear();
            QVERIFY(store.summaries().empty());
        }
        reposco::EncryptedDatabase database(path, key);
        QCOMPARE(database.scalar("PRAGMA user_version"), std::string("2"));
        QCOMPARE(database.scalar("SELECT count(*) FROM history"), std::string("0"));
    }
    void documentNeverReadsAnExistingLocalResource() {
        QTemporaryDir directory;
        QImage image(2, 2, QImage::Format_ARGB32);
        image.fill(Qt::red);
        const auto path = directory.filePath("canary.png");
        QVERIFY(image.save(path));
        reposco::OfflineDocument document;
        const auto resource =
            document.resource(QTextDocument::ImageResource, QUrl::fromLocalFile(path));
        QVERIFY(!resource.canConvert<QImage>());
        QVERIFY(resource.toByteArray().isEmpty());
    }
    void originalFormatsStaySeparateFromSearchAndIdentity() {
        reposco::ClipboardContent raw;
        raw.text = "Synthetic bold";
        raw.html = htmlClipboard("<b>Synthetic bold</b>");
        raw.rtf = "{\\rtf1\\ansi Synthetic \\b bold\\b0}";
        const auto rich = reposco::normalizeClipboard(raw);
        QVERIFY(rich);
        QCOMPARE(rich->kind, QString("HTML"));
        QCOMPARE(rich->text.canonical, QByteArray("Synthetic bold"));
        const auto decoded = reposco::decodeFormats(rich->formats, rich->text.original);
        QVERIFY(decoded);
        QCOMPARE(decoded->html, raw.html);
        QCOMPARE(decoded->rtf, raw.rtf);
        QCOMPARE(decoded->text, raw.text);
        const auto plain = reposco::normalizeClipboard(reposco::ClipboardContent{raw.text});
        QVERIFY(plain);
        QVERIFY(rich->text.fingerprint != plain->text.fingerprint);
        raw.html = htmlClipboard("<i>Synthetic bold</i>");
        const auto italic = reposco::normalizeClipboard(raw);
        QVERIFY(italic);
        QVERIFY(italic->text.fingerprint != rich->text.fingerprint);
        auto broken = rich->formats;
        broken.chop(1);
        QVERIFY(!reposco::decodeFormats(broken, rich->text.original));
    }
    void exactCodeInHtmlIsExcludedBeforeIdentity() {
        reposco::ClipboardContent raw;
        raw.html = htmlClipboard("<span> 123456 </span>");
        QVERIFY(!reposco::normalizeClipboard(raw));
        raw.html = htmlClipboard("<span>code 123456</span>");
        QVERIFY(reposco::normalizeClipboard(raw));
        raw.text = "123456";
        QVERIFY(!reposco::normalizeClipboard(raw));
        QVERIFY(reposco::normalizeClipboard(raw, false));
        raw.html = "Version:1.0\r\nStartHTML:1\r\nEndHTML:99999999\r\nStartFragment:"
                   "1\r\nEndFragment:2\r\nX";
        QVERIFY(!reposco::normalizeClipboard(raw, false));
    }
    void previewsAreOfflineAndBounded() {
        reposco::ClipboardContent raw;
        raw.html =
            htmlClipboard("<p>Synthetic <b>heading</b></p><img src='file:///C:/forbidden.png'>"
                          "<img src='https://invalid.example/secret.png'>"
                          "<table><tr><td>Item</td><td>Count</td></tr>"
                          "<tr><td>Alpha</td><td>2</td></tr></table>");
        const auto content = reposco::normalizeClipboard(raw);
        QVERIFY(content);
        QCOMPARE(content->kind, QString("Table"));
        const auto decoded = reposco::decodeFormats(content->formats, content->text.original);
        QVERIFY(decoded);
        const auto preview = reposco::makeContentPreview(*decoded);
        QCOMPARE(preview.rows.size(), 2);
        QCOMPARE(preview.rows[1].toStringList(), QStringList({"Alpha", "2"}));
        QVERIFY(preview.text.contains("heading"));
        raw = {};
        raw.text =
            "# Synthetic heading\n\n**Bold**\n\n![remote](https://invalid.example/secret.png)";
        const auto markdown = reposco::makeContentPreview(raw);
        QCOMPARE(markdown.kind, QString("Markdown"));
        QVERIFY(markdown.darkImage.startsWith("data:image/png;base64,"));
        QVERIFY(markdown.lightImage.startsWith("data:image/png;base64,"));
        QVERIFY(markdown.text.size() <= 8192);
    }
    void boundedImagesKeepPixelsAndSupportSearch() {
        QImage image(3, 2, QImage::Format_RGBA8888);
        image.fill(QColor(40, 120, 220, 170));
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        reposco::ClipboardContent raw;
        raw.png = buffer.data();
        const auto content = reposco::normalizeClipboard(raw);
        QVERIFY(content);
        QCOMPARE(content->kind, QString("Image"));
        QVERIFY(content->text.original.isEmpty());
        QVERIFY(content->text.canonical.contains("3 x 2"));
        const auto preview = reposco::makeContentPreview(raw);
        QVERIFY(preview.darkImage.startsWith("data:image/png;base64,"));
        auto invalid = raw;
        invalid.png = "not an image";
        QVERIFY(!reposco::normalizeClipboard(invalid));
        raw.png = QByteArray(32 * 1024 * 1024 + 1, 'x');
        QVERIFY(!reposco::normalizeClipboard(raw));
    }
};
QTEST_MAIN(ContentTests)
#include "ContentTests.moc"
