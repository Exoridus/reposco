#include "NativeClipboardMonitor.h"

#include "core/CapturePolicy.h"
#include "core/ContentPolicy.h"
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <Windows.h>
#include <atomic>
#include <cstring>
#include <limits>
#include <objbase.h>

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
    void stop() {
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
        State state{*this, excluded, ignore, permission};
        const auto window =
            CreateWindowExW(0, L"STATIC", L"Reposco Clipboard Monitor", 0, 0, 0, 0, 0, HWND_MESSAGE,
                            nullptr, GetModuleHandleW(nullptr), nullptr);
        bool listening = false;
        if (window && excluded && ignore && permission) {
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
        DWORD lastSequence = 0;
        quint64 generation = 0;
        quint64 token = 0;
        bool inFlight = false;
        bool pending = false;
        int attempts = 0;

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
            TextCapture captured;
            try {
                NativeTextSource source(excluded, ignore, permission,
                                        thread.excludedApplications());
                captured = captureAllowedText(source);
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
            if (captured.decision == TextCaptureDecision::Accepted) {
                if (token == std::numeric_limits<quint64>::max()) {
                    emit thread.monitor_.failure(
                        QStringLiteral("Clipboard event sequence exhausted."));
                    thread.stop();
                    return;
                }
                inFlight = true;
                ++token;
                emit thread.monitor_.captured(QString::fromStdU16String(captured.text), token,
                                              generation);
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
void NativeClipboardMonitor::stop() {
    thread_->stop();
}
} // namespace reposco
