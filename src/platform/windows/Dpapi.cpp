#include "Dpapi.h"

#include <Windows.h>
#include <dpapi.h>

#include <limits>
#include <memory>

namespace reposco {
namespace {
struct FreeBlob {
    void operator()(unsigned char* bytes) const {
        LocalFree(bytes);
    }
};

std::optional<QByteArray> transform(const QByteArray& input, bool protect) {
    if (input.isEmpty() || input.size() > std::numeric_limits<DWORD>::max()) {
        return std::nullopt;
    }
    // DPAPI's input parameter is mutable for ABI compatibility; it does not modify the bytes.
    DATA_BLOB source{static_cast<DWORD>(input.size()),
                     reinterpret_cast<unsigned char*>(const_cast<char*>(input.constData()))};
    DATA_BLOB destination{};
    const bool success = protect
                             ? CryptProtectData(&source, L"Reposco key", nullptr, nullptr, nullptr,
                                                CRYPTPROTECT_UI_FORBIDDEN, &destination) != FALSE
                             : CryptUnprotectData(&source, nullptr, nullptr, nullptr, nullptr,
                                                  CRYPTPROTECT_UI_FORBIDDEN, &destination) != FALSE;
    const std::unique_ptr<unsigned char, FreeBlob> allocation(destination.pbData);
    if (!success) {
        return std::nullopt;
    }
    QByteArray result(reinterpret_cast<const char*>(destination.pbData), destination.cbData);
    SecureZeroMemory(destination.pbData, destination.cbData);
    return result;
}
} // namespace

std::optional<QByteArray> protectKey(const QByteArray& key) {
    return transform(key, true);
}
std::optional<QByteArray> unprotectKey(const QByteArray& envelope) {
    return transform(envelope, false);
}
} // namespace reposco
