#include "GlobalHotkey.h"

#include <QCoreApplication>
#include <QThread>
#include <Windows.h>

namespace reposco {
namespace {
constexpr int kHotkeyId = 0x5250;
}

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}
GlobalHotkey::~GlobalHotkey() {
    unregister();
    QCoreApplication::instance()->removeNativeEventFilter(this);
}
bool GlobalHotkey::registerDefault() {
    Q_ASSERT(QThread::currentThread() == thread());
    if (registered_) {
        return true;
    }
    return registerShortcut(MOD_WIN | MOD_ALT, 'V');
}
bool GlobalHotkey::registerShortcut(unsigned int modifiers, unsigned int key) {
    Q_ASSERT(QThread::currentThread() == thread());
    unregister();
    registered_ = RegisterHotKey(nullptr, kHotkeyId, modifiers | MOD_NOREPEAT, key) != FALSE;
    error_code_ = registered_ ? ERROR_SUCCESS : GetLastError();
    return registered_;
}
void GlobalHotkey::unregister() {
    Q_ASSERT(QThread::currentThread() == thread());
    if (registered_) {
        UnregisterHotKey(nullptr, kHotkeyId);
        registered_ = false;
    }
}
unsigned long GlobalHotkey::errorCode() const {
    return error_code_;
}
bool GlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
        return false;
    }
    const auto* nativeMessage = static_cast<const MSG*>(message);
    if (registered_ && nativeMessage->message == WM_HOTKEY && nativeMessage->wParam == kHotkeyId) {
        emit activated();
        *result = 0;
        return true;
    }
    return false;
}
} // namespace reposco
