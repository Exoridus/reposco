#include "NativeClipboardMonitor.h"

#include "core/CapturePolicy.h"
#include "core/ContentPolicy.h"
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <Windows.h>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <objbase.h>
#include <vector>

namespace reposco {
bool isApplicationExcluded(const QString& executable, const QStringList& rules) {
    if (rules.isEmpty()) {
        return false;
    }
    if (executable.isEmpty()) {
        return true;
    }
    const auto path = QDir::cleanPath(QDir::fromNativeSeparators(executable));
    for (const auto& rule : rules) {
        if (path.compare(QDir::cleanPath(QDir::fromNativeSeparators(rule)), Qt::CaseInsensitive) ==
            0) {
            return true;
        }
    }
    return false;
}
namespace {
QString clipboardOwnerExecutable() {
    DWORD processId = 0;
    const auto owner = GetClipboardOwner();
    if (!owner || !GetWindowThreadProcessId(owner, &processId) || !processId) {
        return {};
    }
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) {
        return {};
    }
    wchar_t buffer[32768]{};
    DWORD length = static_cast<DWORD>(std::size(buffer));
    const bool resolved = QueryFullProcessImageNameW(process, 0, buffer, &length) != FALSE;
    CloseHandle(process);
    return resolved ? QString::fromWCharArray(buffer, static_cast<qsizetype>(length)) : QString{};
}
constexpr UINT kControlMessage = WM_APP + 1;
constexpr UINT_PTR kRetryTimer = 1;

class LockedGlobal final {
  public:
    explicit LockedGlobal(HGLOBAL handle) : handle_(handle), data_(GlobalLock(handle)) {
    }
    ~LockedGlobal() {
        if (data_) {
            GlobalUnlock(handle_);
        }
    }
    LockedGlobal(const LockedGlobal&) = delete;
    LockedGlobal& operator=(const LockedGlobal&) = delete;
    [[nodiscard]] const void* data() const {
        return data_;
    }

  private:
    HGLOBAL handle_;
    void* data_;
};

class NativeTextSource final : public ClipboardTextSource {
  public:
    NativeTextSource(UINT excluded, UINT ignore, UINT permission, QStringList applications)
        : excluded_(excluded), ignore_(ignore), permission_(permission),
          applications_(std::move(applications)) {
    }
    bool hasExclusionMarker() const override {
        return IsClipboardFormatAvailable(excluded_) || IsClipboardFormatAvailable(ignore_) ||
               (!applications_.isEmpty() &&
                isApplicationExcluded(clipboardOwnerExecutable(), applications_));
    }
    bool hasHistoryPermission() const override {
        return IsClipboardFormatAvailable(permission_);
    }
    std::optional<std::uint32_t> historyPermission() override {
        const auto handle = GetClipboardData(permission_);
        if (!handle || GlobalSize(handle) < sizeof(DWORD) || GlobalSize(handle) > 64) {
            return std::nullopt;
        }
        const LockedGlobal allocation(handle);
        if (!allocation.data()) {
            return std::nullopt;
        }
        DWORD value = 0;
        std::memcpy(&value, allocation.data(), sizeof(value));
        return value;
    }
    std::optional<std::u16string> readText() override {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            return std::nullopt;
        }
        const auto handle = GetClipboardData(CF_UNICODETEXT);
        const SIZE_T bytes = handle ? GlobalSize(handle) : 0;
        if (bytes < sizeof(WCHAR) || bytes > 2 * kMaxTextBytes + sizeof(WCHAR) ||
            bytes % sizeof(WCHAR) != 0) {
            return std::nullopt;
        }
        const LockedGlobal allocation(handle);
        if (!allocation.data()) {
            return std::nullopt;
        }
        const auto* text = static_cast<const WCHAR*>(allocation.data());
        std::size_t length = 0;
        const auto capacity = bytes / sizeof(WCHAR);
        while (length < capacity && text[length] != L'\0') {
            ++length;
        }
        if (length == capacity) {
            return std::nullopt;
        }
        std::u16string result;
        result.reserve(length);
        for (std::size_t index = 0; index < length; ++index) {
            result.push_back(static_cast<char16_t>(text[index]));
        }
        return result;
    }

  private:
    UINT excluded_;
    UINT ignore_;
    UINT permission_;
    QStringList applications_;
};
} // namespace

class NativeClipboardMonitor::Thread final : public QThread {
  public:
    explicit Thread(NativeClipboardMonitor& monitor) : monitor_(monitor) {
    }
    void activate(bool active, quint64 generation) {
        active_.store(false);
        generation_.store(generation);
        active_.store(active);
        wake(kControlMessage);
    }
    void setExcludedApplications(const QStringList& applications) {
        const QMutexLocker guard(&rules_mutex_);
        applications_ = applications;
    }
    QStringList excludedApplications() {
        const QMutexLocker guard(&rules_mutex_);
        return applications_;
    }
    void acknowledge(quint64 token) {
        acknowledged_.store(token);
        wake(kControlMessage);
    }
    bool restoreText(const QString& text, quint64 token) {
        return restoreContent(ClipboardContent{text}, token);
    }
    bool restoreContent(const ClipboardContent& content, quint64 token) {
        if (!window_.load() || isInterruptionRequested() || content.payloadBytes() <= 0 ||
            content.payloadBytes() > kMaxClipboardBytes ||
            content.text.size() > static_cast<qsizetype>(kMaxTextBytes) ||
            !content.text.isValidUtf16() || content.text.contains(QChar::Null)) {
            return false;
        }
        {
            const QMutexLocker guard(&restore_mutex_);
            if (restore_content_) {
                return false;
            }
            restore_content_ = content;
            restore_token_.store(token);
        }
        wake(kControlMessage);
        return true;
    }
    void cancelRestore() {
        restore_token_.store(0);
    }
    void stop() {
        cancelRestore();
        active_.store(false);
        requestInterruption();
        wake(WM_CLOSE);
    }

  protected:
    void run() override {
        const HRESULT initialized = OleInitialize(nullptr);
        if (FAILED(initialized)) {
            emit monitor_.failure(QStringLiteral("Clipboard STA could not be initialized."));
            return;
        }
        const UINT excluded =
            RegisterClipboardFormatW(L"ExcludeClipboardContentFromMonitorProcessing");
        const UINT ignore = RegisterClipboardFormatW(L"Clipboard Viewer Ignore");
        const UINT permission = RegisterClipboardFormatW(L"CanIncludeInClipboardHistory");
        const UINT html = RegisterClipboardFormatW(L"HTML Format");
        const UINT rtf = RegisterClipboardFormatW(L"Rich Text Format");
        const UINT png = RegisterClipboardFormatW(L"PNG");
        State state{*this, excluded, ignore, permission, html, rtf, png};
        const auto window =
            CreateWindowExW(0, L"STATIC", L"Reposco Clipboard Monitor", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            nullptr, GetModuleHandleW(nullptr), nullptr);
        bool listening = false;
        if (window && excluded && ignore && permission && html && rtf && png) {
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&state));
            SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&windowProcedure));
            listening = AddClipboardFormatListener(window) != FALSE;
        }
        if (!listening) {
            if (window) {
                DestroyWindow(window);
            }
            OleUninitialize();
            emit monitor_.failure(QStringLiteral("Clipboard listener could not be registered."));
            return;
        }
        window_.store(window);
        state.lastSequence = GetClipboardSequenceNumber();
        emit monitor_.ready();
        MSG message{};
        while (!isInterruptionRequested()) {
            const int result = static_cast<int>(GetMessageW(&message, nullptr, 0, 0));
            if (result <= 0) {
                if (result < 0) {
                    emit monitor_.failure(QStringLiteral("Clipboard message loop failed."));
                }
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        window_.store(nullptr);
        KillTimer(window, kRetryTimer);
        RemoveClipboardFormatListener(window);
        DestroyWindow(window);
        OleUninitialize();
    }

  private:
    struct State {
        Thread& thread;
        UINT excluded;
        UINT ignore;
        UINT permission;
        UINT html;
        UINT rtf;
        UINT png;
        DWORD lastSequence = 0;
        quint64 generation = 0;
        quint64 token = 0;
        bool inFlight = false;
        bool pending = false;
        int attempts = 0;

        void restore(HWND window) {
            std::optional<ClipboardContent> content;
            quint64 request = 0;
            {
                const QMutexLocker guard(&thread.restore_mutex_);
                content.swap(thread.restore_content_);
                request = thread.restore_token_.load();
            }
            if (!content) {
                return;
            }
            using Allocation = std::unique_ptr<void, decltype(&GlobalFree)>;
            struct Offered {
                UINT format;
                Allocation allocation;
            };
            std::vector<Offered> offered;
            offered.reserve(9);
            const auto offer = [&offered](UINT format, const void* data, SIZE_T bytes) {
                Allocation allocation(GlobalAlloc(GMEM_MOVEABLE, bytes), GlobalFree);
                if (!format || !allocation) {
                    return false;
                }
                auto* destination = GlobalLock(allocation.get());
                if (!destination) {
                    return false;
                }
                std::memcpy(destination, data, bytes);
                GlobalUnlock(allocation.get());
                offered.push_back({format, std::move(allocation)});
                return true;
            };
            constexpr DWORD denied = 0;
            constexpr DWORD marked = 1;
            const auto cloud = RegisterClipboardFormatW(L"CanUploadToCloudClipboard");
            bool prepared = offer(excluded, &marked, sizeof(marked)) &&
                            offer(permission, &denied, sizeof(denied)) &&
                            offer(cloud, &denied, sizeof(denied));
            if (prepared && !content->text.isEmpty()) {
                prepared = offer(CF_UNICODETEXT, content->text.utf16(),
                                 SIZE_T(content->text.size() + 1) * sizeof(WCHAR));
            }
            const std::array<std::pair<UINT, const QByteArray*>, 5> originals = {
                std::pair{html, &content->html}, std::pair{rtf, &content->rtf},
                std::pair{png, &content->png}, std::pair{UINT(CF_DIB), &content->dib},
                std::pair{UINT(CF_DIBV5), &content->dibV5}};
            for (const auto& [format, bytes] : originals) {
                if (!prepared || bytes->isEmpty()) {
                    continue;
                }
                if (format == html || format == rtf) {
                    auto terminated = *bytes;
                    terminated.append('\0');
                    prepared = offer(format, terminated.constData(), SIZE_T(terminated.size()));
                } else {
                    prepared = offer(format, bytes->constData(), SIZE_T(bytes->size()));
                }
            }
            bool success = false;
            DWORD sequence = 0;
            if (request && prepared && OpenClipboard(window)) {
                if (request == thread.restore_token_.load() && !thread.isInterruptionRequested() &&
                    EmptyClipboard()) {
                    // Readers cannot observe a payload before exclusion metadata is published.
                    success = true;
                    for (auto& value : offered) {
                        if (!SetClipboardData(value.format, value.allocation.get())) {
                            success = false;
                            break;
                        }
                        (void)value.allocation.release();
                    }
                }
                CloseClipboard();
                sequence = GetClipboardSequenceNumber();
                success = success && GetClipboardOwner() == window;
            }
            emit thread.monitor_.restored(request, sequence, success);
        }
        std::optional<QByteArray> readFormat(UINT format, qsizetype limit, bool terminated) {
            const auto handle = GetClipboardData(format);
            const auto size = handle ? GlobalSize(handle) : 0;
            if (size == 0 || size > SIZE_T(limit)) {
                return std::nullopt;
            }
            const LockedGlobal allocation(handle);
            if (!allocation.data()) {
                return std::nullopt;
            }
            QByteArray bytes(static_cast<const char*>(allocation.data()), qsizetype(size));
            if (terminated) {
                const auto end = bytes.indexOf('\0');
                if (end >= 0) {
                    bytes.truncate(end);
                }
            }
            return bytes.isEmpty() ? std::nullopt : std::optional(bytes);
        }
        std::optional<ClipboardContent> readContent() {
            NativeTextSource source(excluded, ignore, permission, thread.excludedApplications());
            const auto captured = captureAllowedText(source);
            if (captured.decision != TextCaptureDecision::Accepted &&
                !(captured.decision == TextCaptureDecision::Unsupported &&
                  !IsClipboardFormatAvailable(CF_UNICODETEXT))) {
                return std::nullopt;
            }
            ClipboardContent content;
            content.text = QString::fromStdU16String(captured.text);
            const std::array<std::pair<UINT, QByteArray*>, 5> formats = {
                std::pair{html, &content.html}, std::pair{rtf, &content.rtf},
                std::pair{png, &content.png}, std::pair{UINT(CF_DIB), &content.dib},
                std::pair{UINT(CF_DIBV5), &content.dibV5}};
            for (const auto& [format, destination] : formats) {
                if (!IsClipboardFormatAvailable(format)) {
                    continue;
                }
                const bool markup = format == html || format == rtf;
                auto bytes =
                    readFormat(format, markup ? kMaxMarkupBytes : kMaxClipboardBytes, markup);
                if (!bytes) {
                    return std::nullopt;
                }
                *destination = std::move(*bytes);
                if (content.payloadBytes() > kMaxClipboardBytes) {
                    return std::nullopt;
                }
            }
            return content.payloadBytes() > 0 ? std::optional(content) : std::nullopt;
        }
        void process(HWND window, bool update) {
            if (generation != thread.generation_.load()) {
                generation = thread.generation_.load();
                lastSequence = GetClipboardSequenceNumber();
                pending = false;
                attempts = 0;
            } else if (update) {
                pending = true;
                attempts = 0;
            }
            if (inFlight && thread.acknowledged_.load() == token) {
                inFlight = false;
            }
            if (!thread.active_.load() || thread.isInterruptionRequested() || !pending ||
                inFlight) {
                return;
            }
            const auto sequence = GetClipboardSequenceNumber();
            if (sequence == lastSequence) {
                pending = false;
                return;
            }
            if (!OpenClipboard(window)) {
                if (++attempts < 3) {
                    if (!SetTimer(window, kRetryTimer, 35, nullptr)) {
                        emit thread.monitor_.failure(
                            QStringLiteral("Clipboard retry timer failed."));
                        thread.stop();
                    }
                } else {
                    pending = false;
                    lastSequence = sequence;
                }
                return;
            }
            std::optional<ClipboardContent> captured;
            try {
                captured = readContent();
            } catch (...) {
                CloseClipboard();
                throw;
            }
            CloseClipboard();
            pending = false;
            lastSequence = sequence;
            if (GetClipboardSequenceNumber() != sequence || !thread.active_.load() ||
                thread.generation_.load() != generation || thread.isInterruptionRequested()) {
                return;
            }
            if (captured) {
                if (token == std::numeric_limits<quint64>::max()) {
                    emit thread.monitor_.failure(
                        QStringLiteral("Clipboard event sequence exhausted."));
                    thread.stop();
                    return;
                }
                inFlight = true;
                ++token;
                if (captured->html.isEmpty() && captured->rtf.isEmpty() &&
                    captured->png.isEmpty() && captured->dib.isEmpty() &&
                    captured->dibV5.isEmpty()) {
                    emit thread.monitor_.captured(captured->text, token, generation);
                } else {
                    emit thread.monitor_.capturedContent(std::move(*captured), token, generation);
                }
            }
        }
    };
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wparam,
                                            LPARAM lparam) {
        // Win32 returns the application-owned pointer through its integer-sized userdata slot.
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        auto* state = reinterpret_cast<State*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!state) {
            return DefWindowProcW(window, message, wparam, lparam);
        }
        try {
            if (message == WM_CLOSE) {
                PostQuitMessage(0);
                return 0;
            }
            if (message == WM_TIMER && wparam == kRetryTimer) {
                KillTimer(window, kRetryTimer);
                state->process(window, false);
                return 0;
            }
            if (message == WM_CLIPBOARDUPDATE || message == kControlMessage) {
                if (message == kControlMessage) {
                    state->restore(window);
                }
                state->process(window, message == WM_CLIPBOARDUPDATE);
                return 0;
            }
        } catch (...) {
            // Exceptions cannot cross a Win32 callback boundary, including allocation failures.
            emit state->thread.monitor_.failure(QStringLiteral("Clipboard capture failed safely."));
            state->thread.stop();
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
    void wake(UINT message) const {
        if (const auto window = window_.load()) {
            PostMessageW(window, message, 0, 0);
        }
    }
    QMutex restore_mutex_;
    std::optional<ClipboardContent> restore_content_;
    std::atomic<quint64> restore_token_{0};
    QMutex rules_mutex_;
    QStringList applications_;
    NativeClipboardMonitor& monitor_;
    std::atomic<HWND> window_{nullptr};
    std::atomic<bool> active_{false};
    std::atomic<quint64> generation_{0};
    std::atomic<quint64> acknowledged_{0};
};

NativeClipboardMonitor::NativeClipboardMonitor(QObject* parent)
    : ClipboardMonitor(parent), thread_(std::make_unique<Thread>(*this)) {
    connect(thread_.get(), &QThread::finished, this, &ClipboardMonitor::stopped);
}
NativeClipboardMonitor::~NativeClipboardMonitor() {
    thread_->stop();
    thread_->wait();
}
void NativeClipboardMonitor::start() {
    thread_->start();
}
void NativeClipboardMonitor::setActive(bool active, quint64 generation) {
    thread_->activate(active, generation);
}
void NativeClipboardMonitor::acknowledge(quint64 token) {
    thread_->acknowledge(token);
}
void NativeClipboardMonitor::setExcludedApplications(const QStringList& applications) {
    thread_->setExcludedApplications(applications);
}
bool NativeClipboardMonitor::restoreText(const QString& text, quint64 token) {
    return thread_->restoreText(text, token);
}
bool NativeClipboardMonitor::restoreContent(const ClipboardContent& content, quint64 token) {
    return thread_->restoreContent(content, token);
}
void NativeClipboardMonitor::cancelRestore() {
    thread_->cancelRestore();
}
void NativeClipboardMonitor::stop() {
    thread_->stop();
}
} // namespace reposco
