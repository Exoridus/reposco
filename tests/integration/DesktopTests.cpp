#include "ClipboardFixtures.h"
#include "platform/windows/KeyVault.h"
#include "platform/windows/NativeClipboardMonitor.h"
#include "ui/DesktopSession.h"
#include "ui/HistoryController.h"

#include <QAbstractItemModelTester>
#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QPainter>
#include <QProcess>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <QWidget>
#include <QtTest>
#include <Windows.h>
#include <array>
#include <wingdi.h>

namespace {
class MaterialBackdrop final : public QWidget {
  protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(), QColor("#99b3c0"));
        painter.fillRect(QRect(0, 0, width() / 2, height()), QColor("#c6b69f"));
        painter.fillRect(QRect(0, height() * 2 / 3, width(), height() / 3), QColor("#586d70"));
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#e5ddd0"));
        painter.drawEllipse(QPoint(width() / 2, height() / 3), 150, 150);
        painter.setPen(QColor("#34454c"));
        painter.setFont(QFont("Segoe UI", 26));
        painter.drawText(QRect(40, 40, width() - 80, 100), "SYNTHETIC BACKDROP");
    }
};
QImage captureNativeWindow(QQuickWindow* window) {
    // Qt's native window identifier represents HWND on Windows.
    // NOLINTNEXTLINE(performance-no-int-to-ptr)
    const auto handle = reinterpret_cast<HWND>(window->winId());
    RECT rectangle{};
    if (!GetClientRect(handle, &rectangle)) {
        return {};
    }
    const int width = rectangle.right;
    const int height = rectangle.bottom;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const auto source = GetDC(handle);
    const auto destination = CreateCompatibleDC(source);
    void* pixels = nullptr;
    const auto bitmap = CreateDIBSection(source, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    QImage result;
    if (bitmap && destination) {
        const auto previous = SelectObject(destination, bitmap);
        if (PrintWindow(handle, destination, PW_CLIENTONLY | PW_RENDERFULLCONTENT)) {
            result = QImage(static_cast<const uchar*>(pixels), width, height, QImage::Format_RGB32)
                         .copy();
        }
        SelectObject(destination, previous);
    }
    if (bitmap) {
        DeleteObject(bitmap);
    }
    if (destination) {
        DeleteDC(destination);
    }
    if (source) {
        ReleaseDC(handle, source);
    }
    return result;
}
QStringList lifecycleMessages;
void lifecycleMessageHandler(QtMsgType, const QMessageLogContext& context, const QString& message) {
    if (QString::fromLatin1(context.category) == QStringLiteral("reposco.lifecycle")) {
        lifecycleMessages.append(message);
    }
}
QQuickItem* findVisualItem(QQuickItem* item, const QString& name) {
    if (item->objectName() == name) {
        return item;
    }
    for (auto* child : item->childItems()) {
        if (auto* found = findVisualItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}
class SyntheticDesktopMonitor final : public reposco::ClipboardMonitor {
  public:
    quint64 generation = 0;
    quint64 restoreToken = 0;
    bool restoreText(const QString&, quint64 token) override {
        restoreToken = token;
        return true;
    }
    void start() override {
        emit ready();
    }
    void setActive(bool, quint64 epoch) override {
        generation = epoch;
    }
    void acknowledge(quint64) override {
    }
    void stop() override {
        emit stopped();
    }
};
} // namespace

class DesktopTests : public QObject {
    Q_OBJECT
  private slots:
    void twoSurfacesRouteWithoutResizingOrQuitting() {
        QTest::failOnWarning();
        QTemporaryDir directory;
        reposco::HistoryController history(directory.path(),
                                           std::make_unique<SyntheticDesktopMonitor>());
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        auto* quick = session.window();
        auto* workspace = qvariant_cast<QQuickWindow*>(session.property("workspaceWindow"));
        QVERIFY(workspace);
        QVERIFY(workspace != quick);
        QCOMPARE(workspace->property("historyController"), quick->property("historyController"));
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowActive(quick));
        QVERIFY(quick->findChild<QQuickItem*>("paletteSearch")->hasActiveFocus());
        QVERIFY(!quick->findChild<QQuickItem*>("contextPreview"));
        const auto quickSize = quick->size();
        QVERIFY(QMetaObject::invokeMethod(session.window(), "expandHistory"));
        QVERIFY(QTest::qWaitForWindowActive(workspace));
        QVERIFY(workspace->isVisible());
        QVERIFY(!quick->isVisible());
        const auto workspaceSize = workspace->size();
        QVERIFY(QMetaObject::invokeMethod(workspace, "openSettings"));
        QVERIFY(workspace->property("settingsOpen").toBool());
        QCOMPARE(workspace->size(), workspaceSize);
        QTest::keyClick(workspace, Qt::Key_Escape);
        QTRY_VERIFY(!workspace->property("settingsOpen").toBool());
        QVERIFY(workspace->isVisible());
        QTest::keyClick(workspace, Qt::Key_Escape);
        QVERIFY(workspace->isVisible());
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowActive(quick));
        QVERIFY(workspace->isVisible());
        QCOMPARE(quick->size(), quickSize);
        QTest::keyClick(quick, Qt::Key_Escape);
        QTRY_VERIFY(!quick->isVisible());
        QVERIFY(workspace->isVisible());
        workspace->close();
        QVERIFY(!workspace->isVisible());
        QVERIFY(!QApplication::quitOnLastWindowClosed());
    }

    void syntheticPresentationSamples() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        const auto output = qEnvironmentVariable("REPOSCO_VISUAL_OUTPUT");
        MaterialBackdrop backdrop;
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
            const auto area = session.window()->screen()->availableGeometry();
            backdrop.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
            backdrop.setGeometry(area.adjusted(20, 20, -20, -20));
            backdrop.show();
            QVERIFY(QTest::qWaitForWindowExposed(backdrop.windowHandle()));
        }
        reposco::HistoryModel model;
        const auto now = QDateTime::currentMSecsSinceEpoch();
        model.replace(
            {{1, "Design review notes\nSearch comes first. Keep history within easy reach.", now,
              180},
             {2, "https://doc.qt.io/qt-6/qtquick-index.html", now - 120000, 44},
             {3, "Release checklist · synthetic example", now - 420000, 80, "Markdown"},
             {4, "Landscape study · synthetic image", now - 960000, 1800, "Image"},
             {5, "Project overview · three rows", now - 1800000, 110, "Table"}});
        auto* workspace = session.workspaceWindow();
        auto* list = workspace->findChild<QQuickItem*>("historyList");
        auto* quickList = session.window()->findChild<QQuickItem*>("historyList");
        auto* preview = workspace->findChild<QQuickItem*>("contextPreview");
        auto* tokens = session.styleTokens();
        auto* sampleWindow = session.window();
        QVERIFY(list && preview && tokens);
        list->setProperty("model", QVariant::fromValue<QAbstractItemModel*>(&model));
        list->setProperty("currentIndex", 0);
        quickList->setProperty("model", QVariant::fromValue<QAbstractItemModel*>(&model));
        quickList->setProperty("currentIndex", 0);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowExposed(session.window()));
        QVERIFY(QTest::qWaitForWindowActive(session.window()));
        const auto applyPreview = [&](const reposco::ClipboardContent& content, bool dark) {
            const auto rendered = reposco::makeContentPreview(content);
            preview->setProperty("preview", rendered.text);
            preview->setProperty("rows", rendered.rows);
            preview->setProperty("imageSource", dark ? rendered.darkImage : rendered.lightImage);
            preview->setProperty("truncated", rendered.truncated);
        };
        const auto snapshot = [&](const QString& name) {
            auto* previewImage = sampleWindow->findChild<QQuickItem*>("previewImage");
            if (previewImage && previewImage->isVisible()) {
                QTRY_COMPARE_WITH_TIMEOUT(previewImage->property("status").toInt(), 1, 5000);
            }
            QTest::qWait(180);
            sampleWindow->grabWindow();
            QTest::qWait(60);
            const auto rendered = sampleWindow->grabWindow();
            QVERIFY(!rendered.isNull());
            if (!output.isEmpty()) {
                QVERIFY(rendered.save(output + "/" + name + "-qt.png"));
                const auto native = captureNativeWindow(sampleWindow);
                if (!native.isNull()) {
                    QVERIFY(native.save(output + "/" + name + "-native.png"));
                }
                // Capture only the foreground fixture rectangle; never sample a user's backdrop.
                // Qt's native window identifier represents HWND on Windows.
                // NOLINTNEXTLINE(performance-no-int-to-ptr)
                const auto handle = reinterpret_cast<HWND>(sampleWindow->winId());
                if (GetForegroundWindow() == handle) {
                    const auto position =
                        sampleWindow->position() - sampleWindow->screen()->geometry().topLeft();
                    const auto composed = sampleWindow->screen()->grabWindow(
                        0, position.x(), position.y(), sampleWindow->width(),
                        sampleWindow->height());
                    if (!composed.isNull()) {
                        QVERIFY(composed.save(output + "/" + name + ".png"));
                    }
                }
            }
        };
        tokens->setProperty("appearance", "Dark");
        snapshot("quick-dark");
        tokens->setProperty("appearance", "Light");
        snapshot("quick-light");
        session.setAcrylicEnabled(false);
        snapshot("quick-fallback-light");
        tokens->setProperty("appearance", "Dark");
        snapshot("quick-fallback-dark");
        session.setAcrylicEnabled(true);
        session.showWorkspace();
        QVERIFY(QTest::qWaitForWindowActive(workspace));
        sampleWindow = workspace;
        reposco::ClipboardContent content;
        content.text =
            "Design review notes\n\nSearch comes first. Keep history within easy reach.\n"
            "Use restrained motion and clear, quiet controls.";
        applyPreview(content, true);
        snapshot("workspace-dark");
        tokens->setProperty("appearance", "Light");
        applyPreview(content, false);
        snapshot("workspace-light");
        session.setAcrylicEnabled(false);
        snapshot("workspace-fallback-light");
        tokens->setProperty("appearance", "Dark");
        applyPreview(content, true);
        snapshot("workspace-fallback-dark");
        session.setAcrylicEnabled(true);
        list->setProperty("currentIndex", 2);
        content = {};
        content.text = "# Release checklist\n\n**A little clarity goes a long way.**\n\n"
                       "- Keep the original content\n- Preview safely, entirely offline\n- Make "
                       "room for focus";
        applyPreview(content, true);
        snapshot("markdown-dark");
        list->setProperty("currentIndex", 3);
        content = {};
        QImage image(640, 360, QImage::Format_RGB32);
        {
            QPainter painter(&image);
            painter.fillRect(image.rect(), QColor("#e9e2d5"));
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#c89668"));
            painter.drawEllipse(QPoint(475, 90), 35, 35);
            painter.setBrush(QColor("#87999a"));
            painter.drawEllipse(QRect(-60, 150, 560, 320));
            painter.setBrush(QColor("#435c62"));
            painter.drawEllipse(QRect(220, 175, 600, 330));
        }
        QBuffer buffer(&content.png);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        applyPreview(content, true);
        snapshot("image-dark");
        list->setProperty("currentIndex", 4);
        content = {};
        content.text =
            "Project\tStatus\tOwner\nPalette\tIn review\tDesign\nHistory\tReady\tDesktop";
        content.html = reposco::testing::htmlClipboard(
            "<table><tr><th>Project</th><th>Status</th><th>Owner</th></tr>"
            "<tr><td>Palette</td><td>In review</td><td>Design</td></tr>"
            "<tr><td>History</td><td>Ready</td><td>Desktop</td></tr></table>");
        applyPreview(content, true);
        snapshot("table-dark");
        QVERIFY(QMetaObject::invokeMethod(workspace, "openSettings"));
        snapshot("settings-dark");
        tokens->setProperty("appearance", "Light");
        snapshot("settings-light");
        if (!output.isEmpty()) {
            QFile metadata(output + "/rendering.txt");
            QVERIFY(metadata.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream stream(&metadata);
            stream << "Fixture: production Main.qml, WorkspaceWindow.qml and components, synthetic "
                      "summaries and "
                      "offline previews.\n"
                   << "Storage and capture are unavailable in this rendering-only fixture.\n"
                   << "DWM transient backdrop applied: " << session.acrylicActive() << "\n"
                   << "DWM main-window backdrop applied: " << session.micaActive() << "\n"
                   << "High Contrast: " << session.highContrast() << "\n"
                   << "Device pixel ratio: " << session.window()->devicePixelRatio() << "\n"
                   << "Logical width: " << session.window()->width() << "\n"
                   << "Qt scene and PrintWindow captures do not prove visible desktop blur.\n"
                   << "Unsuffixed captures are foreground screen crops, when available.\n";
        }
    }
    void boundedStackHandlesRapidNavigation() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        reposco::HistoryModel model;
        std::vector<reposco::HistorySummary> rows;
        rows.reserve(1000);
        for (int index = 0; index < 1000; ++index) {
            rows.push_back({index + 1, QString("Synthetic entry %1").arg(index), 1, 30});
        }
        model.replace(std::move(rows));
        auto* list = session.window()->findChild<QQuickItem*>("historyList");
        QVERIFY(list);
        list->setProperty("model", QVariant::fromValue<QAbstractItemModel*>(&model));
        list->setProperty("currentIndex", 0);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowActive(session.window()));
        session.focusSearch();
        for (int index = 0; index < 40; ++index) {
            QTest::keyClick(session.window(), Qt::Key_Down);
        }
        QCOMPARE(list->property("currentIndex").toInt(), 40);
        QCOMPARE(session.window()->property("selectedExcerpt").toString(),
                 QString("Synthetic entry 40"));
        session.window()->setProperty("reducedMotion", true);
        QVERIFY(
            QMetaObject::invokeMethod(session.window(), "moveSelection", Q_ARG(QVariant, 1000)));
        QCOMPARE(list->property("currentIndex").toInt(), 999);
        QCOMPARE(session.window()->property("selectedExcerpt").toString(),
                 QString("Synthetic entry 999"));
        QVERIFY(!session.window()->grabWindow().isNull());
    }
    void summaryMovesPreserveDelegateIdentity() {
        reposco::HistoryModel model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.replace({{1, "Alpha", 10, 5}, {2, "Beta", 20, 4}, {3, "Gamma", 30, 5}});
        QPersistentModelIndex beta(model.index(1, 0));
        QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
        QSignalSpy moves(&model, &QAbstractItemModel::rowsMoved);
        model.replace({{2, "Beta updated", 40, 12}, {4, "Delta", 50, 5}, {1, "Alpha", 10, 5}});
        QCOMPARE(resets.count(), 0);
        QCOMPARE(moves.count(), 1);
        QVERIFY(beta.isValid());
        QCOMPARE(beta.row(), 0);
        QCOMPARE(beta.data(reposco::HistoryModel::ExcerptRole).toString(), QString("Beta updated"));
        QCOMPARE(model.rowCount(), 3);
        model.replace({});
        QVERIFY(!beta.isValid());
        QCOMPARE(model.rowCount(), 0);
    }
    void sharedSelectionSurvivesExpandAndReorder() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        history.setCaptureActive(true);
        quint64 token = 0;
        for (const auto* text : {"Synthetic alpha", "Synthetic beta", "Synthetic gamma"}) {
            emit source->captured(text, ++token, source->generation);
            QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), int(token), 5000);
        }
        history.selectRow(1);
        const auto selected = history.selectedEntryId();
        QTRY_COMPARE(history.preview(), QString("Synthetic beta"));
        emit source->captured("Synthetic delta", ++token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 4, 5000);
        QCOMPARE(history.selectedEntryId(), selected);
        QCOMPARE(history.selectedRow(), 2);
        history.setCaptureActive(false);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowActive(session.window()));
        auto* quickList = session.window()->findChild<QQuickItem*>("historyList");
        QCOMPARE(quickList->property("currentIndex").toInt(), 2);
        QVERIFY(QMetaObject::invokeMethod(session.window(), "expandHistory"));
        QVERIFY(QTest::qWaitForWindowActive(session.workspaceWindow()));
        auto* workspaceList = session.workspaceWindow()->findChild<QQuickItem*>("historyList");
        QCOMPARE(workspaceList->property("currentIndex").toInt(), 2);
        QCOMPARE(history.selectedEntryId(), selected);
        QCOMPARE(source->restoreToken, quint64{0});
        QSignalSpy stopped(&history, &reposco::HistoryController::stopped);
        history.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void settingsCannotTriggerHistoryReplay() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        QVERIFY(!history.captureActive());
        history.setCaptureActive(true);
        emit source->captured("Synthetic replay guard", 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 1, 5000);
        history.setCaptureActive(false);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowActive(session.window()));
        QTRY_COMPARE_WITH_TIMEOUT(history.preview(), QString("Synthetic replay guard"), 5000);
        QVERIFY(QMetaObject::invokeMethod(session.window(), "openSettings"));
        auto* workspace = session.workspaceWindow();
        QVERIFY(QTest::qWaitForWindowActive(workspace));
        auto* settings = workspace->findChild<QQuickItem*>("settingsView");
        QVERIFY(settings);
        settings->setProperty("showApplications", true);
        auto* paths = workspace->findChild<QQuickItem*>("applicationPaths");
        QVERIFY(paths);
        paths->forceActiveFocus();
        QTest::keyClick(workspace, Qt::Key_Return);
        session.replaySelected(true);
        session.copySelected();
        QTest::qWait(100);
        QCOMPARE(source->restoreToken, quint64{0});
        QVERIFY(!history.captureActive());
        QTest::keyClick(workspace, Qt::Key_Escape);
        QVERIFY(workspace->isVisible());
        QVERIFY(!workspace->property("settingsOpen").toBool());
        session.copySelected(true);
        QTRY_VERIFY_WITH_TIMEOUT(source->restoreToken > 0, 5000);
        emit source->restored(source->restoreToken, GetClipboardSequenceNumber(), true);
        QVERIFY(workspace->isVisible());
        history.shutdown();
    }
    void dispatcherHotkeyAcceptsAbsentResultPointer() {
        reposco::GlobalHotkey hotkey;
        QVERIFY(hotkey.registerDefault());
        QSignalSpy activated(&hotkey, &reposco::GlobalHotkey::activated);
        MSG message{};
        message.message = WM_HOTKEY;
        message.wParam = 0x5250;
        QVERIFY(hotkey.nativeEventFilter("windows_dispatcher_MSG", &message, nullptr));
        QCOMPARE(activated.count(), 1);
    }
    void coordinatedNativeHotkeyKeepsPaletteOpen() {
        if (qEnvironmentVariable("REPOSCO_COORDINATED_DESKTOP") != "1") {
            QSKIP("Requires a currently coordinated interactive desktop.");
        }
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        QCOMPARE(session.hotkeyError(), 0UL);
        QQuickWindow target;
        target.setTitle("Reposco synthetic hotkey target");
        target.resize(320, 180);
        target.show();
        auto* search = session.window()->findChild<QQuickItem*>("paletteSearch");
        QVERIFY(search);
        const auto nativeHandle = [](QQuickWindow* window) {
            // Qt exposes the native HWND through a platform-neutral window identifier.
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            return reinterpret_cast<HWND>(window->winId());
        };
        const auto sendShortcut = [] {
            std::array<INPUT, 6> input{};
            constexpr std::array<WORD, 6> keys{VK_LWIN, VK_MENU, 'V', 'V', VK_MENU, VK_LWIN};
            for (std::size_t index = 0; index < input.size(); ++index) {
                input[index].type = INPUT_KEYBOARD;
                input[index].ki.wVk = keys[index];
                input[index].ki.dwFlags = index >= 3 ? KEYEVENTF_KEYUP : 0;
            }
            return SendInput(static_cast<UINT>(input.size()), input.data(), sizeof(INPUT)) ==
                   input.size();
        };
        for (int iteration = 0; iteration < 3; ++iteration) {
            target.raise();
            target.requestActivate();
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), nativeHandle(&target), 3000);
            for (const auto key : {VK_LWIN, VK_RWIN, VK_MENU, VK_CONTROL, VK_SHIFT}) {
                QVERIFY((GetAsyncKeyState(key) & 0x8000) == 0);
            }
            QVERIFY(sendShortcut());
            QTRY_VERIFY_WITH_TIMEOUT(session.window()->isVisible(), 3000);
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), nativeHandle(session.window()), 3000);
            QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 3000);
            QTest::qWait(500);
            QVERIFY(session.window()->isVisible());
            QVERIFY(session.trayAvailable());
            QVERIFY(sendShortcut());
            QVERIFY(sendShortcut());
            QTest::qWait(300);
            QVERIFY(session.window()->isVisible());
            QCOMPARE(GetForegroundWindow(), nativeHandle(session.window()));
            std::array<INPUT, 2> escape{};
            for (auto& event : escape) {
                event.type = INPUT_KEYBOARD;
                event.ki.wVk = VK_ESCAPE;
            }
            escape[1].ki.dwFlags = KEYEVENTF_KEYUP;
            QCOMPARE(SendInput(static_cast<UINT>(escape.size()), escape.data(), sizeof(INPUT)),
                     static_cast<UINT>(escape.size()));
            QTRY_VERIFY_WITH_TIMEOUT(!session.window()->isVisible(), 3000);
        }
    }
    void delayedReplayDoesNotReclaimFocusFromAnotherWindow() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        history.setCaptureActive(true);
        emit source->captured("synthetic delayed replay", 1, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 1, 5000);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowActive(session.window()));
        history.selectRow(0);
        QTRY_COMPARE_WITH_TIMEOUT(history.preview(), QString("synthetic delayed replay"), 5000);
        session.replaySelected(false);
        QTRY_VERIFY_WITH_TIMEOUT(source->restoreToken > 0, 5000);
        QQuickWindow other;
        other.setTitle("Reposco synthetic focus change");
        other.resize(320, 180);
        other.show();
        other.requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(&other));
        emit source->restored(source->restoreToken, GetClipboardSequenceNumber(), true);
        QVERIFY(history.replayStatus().contains("Focus or clipboard changed"));
        QVERIFY(other.isActive());
        QVERIFY(session.window()->isVisible());
        history.shutdown();
    }
    void coordinatedTextReplayIntoOwnedNativeFixture() {
        if (qEnvironmentVariable("REPOSCO_COORDINATED_DESKTOP") != "1") {
            QSKIP("Requires a currently coordinated interactive desktop.");
        }
        QTemporaryDir directory;
        const auto original = QString("Synthetic replay\r\nSpacing  preserved  ");
        {
            const auto key = reposco::loadDatabaseKey(directory.path(), false);
            reposco::HistoryStore store(directory.filePath("history.db").toStdString(),
                                        key->bytes());
            const auto content = reposco::normalizeText(original);
            QVERIFY(content);
            store.record(*content, QDateTime::currentMSecsSinceEpoch());
        }
        reposco::HistoryController history(directory.path(),
                                           std::make_unique<reposco::NativeClipboardMonitor>());
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        QCOMPARE(session.hotkeyError(), 0UL);
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        QVERIFY(!history.captureActive());
        QProcess fixture;
        fixture.start(QCoreApplication::applicationFilePath(), {"--paste-fixture"});
        QVERIFY(fixture.waitForStarted(3000));
        QVERIFY(fixture.waitForReadyRead(3000));
        bool valid = false;
        const auto identifier = fixture.readLine().trimmed().toULongLong(&valid);
        QVERIFY(valid);
        // The child returns only its synthetic EDIT handle; no user windows are queried.
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        const auto target = reinterpret_cast<HWND>(static_cast<quintptr>(identifier));
        DWORD processId = 0;
        QVERIFY(GetWindowThreadProcessId(target, &processId) != 0);
        QCOMPARE(static_cast<qint64>(processId), fixture.processId());
        const auto sendKeys = [](std::initializer_list<WORD> keys) {
            std::vector<INPUT> input;
            for (const auto key : keys) {
                INPUT event{};
                event.type = INPUT_KEYBOARD;
                event.ki.wVk = key;
                input.push_back(event);
            }
            const auto pressed = input;
            for (auto it = pressed.rbegin(); it != pressed.rend(); ++it) {
                auto event = *it;
                event.ki.dwFlags = KEYEVENTF_KEYUP;
                input.push_back(event);
            }
            return SendInput(static_cast<UINT>(input.size()), input.data(), sizeof(INPUT)) ==
                   input.size();
        };
        const auto hasPastedOriginal = [&] {
            wchar_t text[256]{};
            DWORD_PTR returned = 0;
            if (!SendMessageTimeoutW(target, WM_GETTEXT, std::size(text),
                                     reinterpret_cast<LPARAM>(text), SMTO_ABORTIFHUNG, 300,
                                     &returned)) {
                return false;
            }
            return QString::fromWCharArray(text) == original;
        };
        for (const int mode : {0, 1, 2}) {
            qInfo("Synthetic replay mode: %d", mode);
            session.showWindow();
            QVERIFY(QTest::qWaitForWindowActive(session.window()));
            session.window()->hide();
            QVERIFY(SetForegroundWindow(target));
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), target, 3000);
            QVERIFY(sendKeys({VK_LWIN, VK_MENU, 'V'}));
            QTRY_VERIFY_WITH_TIMEOUT(session.window()->isVisible(), 3000);
            // NOLINTNEXTLINE(performance-no-int-to-ptr)
            const auto palette = reinterpret_cast<HWND>(session.window()->winId());
            QTRY_COMPARE_WITH_TIMEOUT(GetForegroundWindow(), palette, 3000);
            QTRY_VERIFY_WITH_TIMEOUT(session.window()->isActive(), 3000);
            history.selectRow(0);
            QTRY_VERIFY2_WITH_TIMEOUT(
                history.preview() == original,
                qPrintable(QString("Mode %1: rows=%2, queryLength=%3, selected=%4, active=%5")
                               .arg(mode)
                               .arg(history.model()->rowCount())
                               .arg(history.searchQuery().size())
                               .arg(history.selectedEntryId())
                               .arg(session.window()->isActive())),
                3000);
            auto* search = session.window()->findChild<QQuickItem*>("paletteSearch");
            QVERIFY(search);
            QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 3000);
            if (mode == 0) {
                QVERIFY(sendKeys({VK_CONTROL, VK_RETURN}));
                QTRY_COMPARE_WITH_TIMEOUT(history.replayStatus(), QString("Copied content"), 3000);
                QTRY_VERIFY(!session.window()->isVisible());
                QVERIFY(!hasPastedOriginal());
                SendMessageW(target, WM_PASTE, 0, 0);
            } else if (mode == 1) {
                QVERIFY(sendKeys({VK_RETURN}));
            } else {
                QVERIFY(sendKeys({VK_SHIFT, VK_RETURN}));
            }
            QTRY_VERIFY2_WITH_TIMEOUT(
                hasPastedOriginal(),
                qPrintable(QString("Mode %1: %2").arg(mode).arg(history.replayStatus())), 3000);
            if (mode != 0) {
                QVERIFY(!session.window()->isVisible());
                QCOMPARE(history.replayStatus(), QString("Paste sent"));
            }
            QCOMPARE(history.model()->rowCount(), 1);
            SendMessageW(target, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
        }
        history.shutdown();
        fixture.kill();
        QVERIFY(fixture.waitForFinished(3000));
    }
    void syntheticVisualStatesAndKeyboardSelection() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        session.showWindow();
        QVERIFY(QTest::qWaitForWindowExposed(session.window()));
        const auto output = qEnvironmentVariable("REPOSCO_VISUAL_OUTPUT");
        if (!output.isEmpty()) {
            QVERIFY(QDir().mkpath(output));
        }
        auto* sampleWindow = session.window();
        const auto snapshot = [&](const QString& name) {
            QTest::qWait(150);
            const auto image = sampleWindow->grabWindow();
            QVERIFY(!image.isNull());
            if (!output.isEmpty()) {
                QVERIFY(image.save(output + "/" + name + ".png"));
            }
        };
        snapshot("empty-dark");
        session.window()->setProperty("darkMode", false);
        snapshot("empty-light");
        history.setCaptureActive(true);
        quint64 token = 0;
        for (const auto* text : {"Meeting notes: synthetic fixtures only", "Architecture notes",
                                 "https://github.com/Exoridus",
                                 "const settings = {\n  encrypted: true,\n  history: true\n};"}) {
            emit source->captured(text, ++token, source->generation);
            auto* list = session.window()->findChild<QQuickItem*>("historyList");
            QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), static_cast<int>(token),
                                      5000);
        }
        history.setCaptureActive(false);
        history.selectRow(0);
        QTRY_VERIFY_WITH_TIMEOUT(!history.preview().isEmpty(), 5000);
        session.window()->setProperty("darkMode", true);
        snapshot("populated-dark");
        auto* search = session.window()->findChild<QQuickItem*>("paletteSearch");
        auto* list = session.window()->findChild<QQuickItem*>("historyList");
        session.focusSearch();
        QTest::keyClick(session.window(), Qt::Key_Down);
        QCOMPARE(list->property("currentIndex").toInt(), 1);
        QTest::keyClick(session.window(), Qt::Key_Up);
        QCOMPARE(list->property("currentIndex").toInt(), 0);
        search->setProperty("text", "architecture");
        QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(history.preview(), QString("Architecture notes"), 5000);
        snapshot("search-dark");
        session.window()->setProperty("darkMode", false);
        snapshot("search-light");
        search->setProperty("text", "");
        QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), 4, 5000);
        history.selectRow(0);
        QTRY_VERIFY_WITH_TIMEOUT(history.preview().startsWith("const settings"), 5000);
        snapshot("populated-light");
        session.showWorkspace();
        sampleWindow = session.workspaceWindow();
        QVERIFY(QTest::qWaitForWindowActive(sampleWindow));
        history.setCaptureActive(true);
        reposco::ClipboardContent rich;
        rich.text = "Item\tCount\nAlpha\t2";
        rich.html = reposco::testing::htmlClipboard(
            "<table><tr><th>Item</th><th>Count</th></tr><tr><td>Alpha</td><td>2</td></tr></table>");
        emit source->capturedContent(rich, ++token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 5, 5000);
        history.selectRow(0);
        QTRY_COMPARE_WITH_TIMEOUT(history.previewKind(), QString("Table"), 5000);
        snapshot("table-light");
        session.window()->setProperty("darkMode", true);
        snapshot("table-dark");
        rich = {};
        rich.text = "# Synthetic heading\n\n**Offline preview**\n\n- original content preserved";
        emit source->capturedContent(rich, ++token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 6, 5000);
        history.selectRow(0);
        QTRY_VERIFY_WITH_TIMEOUT(!history.darkPreviewImage().isEmpty(), 5000);
        snapshot("markdown-dark");
        session.window()->setProperty("darkMode", false);
        snapshot("markdown-light");
        rich = {};
        QImage fixture(120, 72, QImage::Format_ARGB32);
        fixture.fill(QColor(50, 130, 210));
        QBuffer buffer(&rich.png);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(fixture.save(&buffer, "PNG"));
        emit source->capturedContent(rich, ++token, source->generation);
        QTRY_COMPARE_WITH_TIMEOUT(history.model()->rowCount(), 7, 5000);
        history.selectRow(0);
        QTRY_COMPARE_WITH_TIMEOUT(history.previewKind(), QString("Image"), 5000);
        snapshot("image-light");
        session.window()->setProperty("darkMode", true);
        auto* previewImage = sampleWindow->findChild<QQuickItem*>("previewImage");
        QVERIFY(previewImage);
        QTest::qWait(200);
        const auto painted = sampleWindow->grabWindow();
        const auto scale = qreal(painted.width()) / sampleWindow->width();
        const auto origin = previewImage->mapToScene(QPointF{});
        const int scanY = int((origin.y() + previewImage->height() / 2) * scale);
        int visiblePixels = 0;
        for (int x = int(origin.x() * scale); x < int((origin.x() + previewImage->width()) * scale);
             ++x) {
            if (painted.pixelColor(x, scanY) == QColor(50, 130, 210)) {
                ++visiblePixels;
            }
        }
        QVERIFY2(visiblePixels >= previewImage->property("paintedWidth").toReal() * scale * 0.8,
                 "Image preview is clipped despite fitting its viewport");
        snapshot("image-dark");
        history.setCaptureActive(false);
        session.window()->setProperty("darkMode", false);
        QMetaObject::invokeMethod(sampleWindow, "openSettings");
        auto* settings = sampleWindow->findChild<QQuickItem*>("settingsView");
        QVERIFY(settings);
        QVERIFY(settings->isVisible());
        snapshot("settings-light");
        session.window()->setProperty("darkMode", true);
        snapshot("settings-dark");
        QVERIFY(QMetaObject::invokeMethod(sampleWindow, "returnToHistory"));
        session.showWindow();
        QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(session.window()->isActive(), 2000);
        QTest::keyClick(session.window(), Qt::Key_Escape);
        QVERIFY(!session.window()->isVisible());
        session.showWindow();
        QTRY_VERIFY_WITH_TIMEOUT(search->hasActiveFocus(), 2000);
        QSignalSpy stopped(&history, &reposco::HistoryController::stopped);
        history.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void nativeHotkeyDispatchIsIdempotentAndDiagnosticsContainNoPayload() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        auto* hotkey = session.findChild<reposco::GlobalHotkey*>();
        QVERIFY(hotkey);
        QVERIFY(session.setShortcut("Win+Alt+V"));
        QVERIFY(!session.setShortcut("unsupported"));
        MSG message{};
        message.message = WM_HOTKEY;
        message.wParam = 0x5250;
        qintptr result = -1;
        lifecycleMessages.clear();
        QLoggingCategory::setFilterRules(QStringLiteral("reposco.lifecycle.info=true"));
        const auto previous = qInstallMessageHandler(lifecycleMessageHandler);
        for (int index = 0; index < 6; ++index) {
            hotkey->nativeEventFilter("windows_generic_MSG", &message, &result);
        }
        session.window()->hide();
        hotkey->nativeEventFilter("windows_dispatcher_MSG", &message, &result);
        qInstallMessageHandler(previous);
        QLoggingCategory::setFilterRules({});
        QLoggingCategory::setFilterRules(QString{});
        QTRY_VERIFY_WITH_TIMEOUT(session.window()->isVisible(), 2000);
        QCOMPARE(result, qintptr{0});
        QVERIFY(!lifecycleMessages.empty());
        for (const auto& line : lifecycleMessages) {
            QVERIFY(line == "palette open requested" || line.startsWith("palette visible ") ||
                    line.startsWith("palette active ") || line == "focus search" ||
                    line == "hotkey activation");
        }
        QTest::qWait(350);
        QVERIFY(session.window()->isVisible());
        session.window()->close();
        QVERIFY(!session.window()->isVisible());
    }
    void settingsStayInWorkspaceAndKeepEditorFocus() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        session.showWorkspace();
        auto* workspace = session.workspaceWindow();
        QVERIFY(QTest::qWaitForWindowActive(workspace));
        auto* search = workspace->findChild<QQuickItem*>("workspaceSearch");
        QVERIFY(search && search->hasActiveFocus());
        const auto size = workspace->size();
        session.showSettings();
        auto* settings = workspace->findChild<QQuickItem*>("settingsView");
        QVERIFY(settings && settings->isVisible());
        QCOMPARE(workspace->size(), size);
        QVERIFY(!search->hasActiveFocus());
        QCOMPARE(workspace->findChildren<QQuickWindow*>().size(), 0);
        auto* paths = workspace->findChild<QQuickItem*>("applicationPaths");
        QVERIFY(paths);
        settings->setProperty("showApplications", true);
        paths->forceActiveFocus();
        session.showWorkspace();
        QVERIFY(paths->hasActiveFocus());
        QTest::keyClick(workspace, Qt::Key_Escape);
        QTRY_VERIFY(!settings->isVisible());
        QVERIFY(workspace->isVisible());
        QVERIFY(search->hasActiveFocus());
        session.styleTokens()->setProperty("appearance", "Light");
        QVERIFY(!workspace->grabWindow().isNull());
        session.styleTokens()->setProperty("appearance", "Dark");
        session.styleTokens()->setProperty("reducedMotion", true);
        QVERIFY(!workspace->grabWindow().isNull());
        workspace->close();
        QVERIFY(!workspace->isVisible());
        session.showWorkspace();
        QTRY_VERIFY(search->hasActiveFocus());
    }
    void trayRequestsKeepWorkspaceVisible() {
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        QAction* show = nullptr;
        for (auto* action : session.findChildren<QAction*>()) {
            if (action->text() == "Open History") {
                show = action;
            }
        }
        QVERIFY(show);
        show->trigger();
        QVERIFY(session.workspaceWindow()->isVisible());
        for (int index = 0; index < 5; ++index) {
            show->trigger();
            QVERIFY(session.workspaceWindow()->isVisible());
        }
        session.workspaceWindow()->close();
        QVERIFY(!session.workspaceWindow()->isVisible());
        show->trigger();
        QVERIFY(session.workspaceWindow()->isVisible());
    }
    void historyControlsAndPlainTextList() {
        QTemporaryDir directory;
        auto monitor = std::make_unique<SyntheticDesktopMonitor>();
        auto* source = monitor.get();
        reposco::HistoryController history(directory.path(), std::move(monitor));
        reposco::DesktopSession session;
        QVERIFY(session.initialize(&history));
        history.start();
        QTRY_VERIFY_WITH_TIMEOUT(history.available(), 5000);
        session.workspaceWindow()->show();
        QVERIFY(QTest::qWaitForWindowExposed(session.workspaceWindow()));
        auto* toggle = session.workspaceWindow()->findChild<QQuickItem*>("captureToggle");
        QVERIFY(toggle);
        QVERIFY(QMetaObject::invokeMethod(session.workspaceWindow(), "openSettings"));
        QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
        QVERIFY(!history.captureActive());
        auto* consent = session.workspaceWindow()->findChild<QQuickItem*>("confirmSettingsAction");
        QVERIFY(consent);
        QVERIFY(QMetaObject::invokeMethod(consent, "clicked"));
        QVERIFY(history.captureActive());
        QVERIFY(QMetaObject::invokeMethod(session.workspaceWindow(), "returnToHistory"));
        emit source->captured("<img src='http://127.0.0.1/secret'> synthetic", 1,
                              source->generation);
        auto* list = session.workspaceWindow()->findChild<QQuickItem*>("historyList");
        QVERIFY(list);
        QTRY_COMPARE_WITH_TIMEOUT(list->property("count").toInt(), 1, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(findVisualItem(list, "historyExcerpt"), 2000);
        auto* excerpt = findVisualItem(list, "historyExcerpt");
        QCOMPARE(excerpt->property("textFormat").toInt(), 0);
        QVERIFY(excerpt->property("text").toString().startsWith("<img"));
        auto* pause = session.findChild<QAction*>("captureAction");
        QVERIFY(pause);
        pause->trigger();
        QVERIFY(!history.captureActive());
        QSignalSpy stopped(&history, &reposco::HistoryController::stopped);
        history.shutdown();
        QTRY_COMPARE_WITH_TIMEOUT(stopped.count(), 1, 5000);
    }
    void windowTrayAndShutdown() {
        QApplication::setQuitOnLastWindowClosed(false);
        reposco::DesktopSession session;
        QVERIFY(session.initialize());
        QVERIFY(session.window());
        QCOMPARE(session.window()->title(), QString("Reposco"));
        session.window()->show();
        QVERIFY(QTest::qWaitForWindowExposed(session.window()));
        QVERIFY(!session.window()->grabWindow().isNull());
        session.window()->close();
        QVERIFY(!session.window()->isVisible());
        QVERIFY(!QApplication::quitOnLastWindowClosed());
        session.toggleWindow();
        QVERIFY(session.window()->isVisible());
        session.toggleWindow();
        QVERIFY(!session.window()->isVisible());
        auto* quitAction = session.findChild<QAction*>("quitAction");
        QVERIFY(quitAction);
        QSignalSpy quitSpy(&session, &reposco::DesktopSession::quitRequested);
        quitAction->trigger();
        QCOMPARE(quitSpy.count(), 1);
        QVERIFY(session.trayAvailable());
    }
};

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    if (application.arguments().contains("--paste-fixture")) {
        const auto edit =
            CreateWindowExW(0, L"EDIT", L"", WS_OVERLAPPEDWINDOW | WS_VISIBLE | ES_MULTILINE, 100,
                            100, 380, 180, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!edit) {
            return 2;
        }
        QTextStream(stdout) << reinterpret_cast<quintptr>(edit) << Qt::endl;
        QTimer focus;
        focus.setInterval(20);
        QObject::connect(&focus, &QTimer::timeout, &application, [edit] {
            if (GetForegroundWindow() == edit && GetFocus() != edit) {
                SetFocus(edit);
            }
        });
        focus.start();
        QTimer::singleShot(30000, &application, &QCoreApplication::quit);
        const auto result = application.exec();
        DestroyWindow(edit);
        return result;
    }
    DesktopTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "DesktopTests.moc"
