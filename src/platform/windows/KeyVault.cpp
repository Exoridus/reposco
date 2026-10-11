#include "KeyVault.h"

#include "Dpapi.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <Windows.h>
#include <algorithm>
#include <bcrypt.h>
#include <stdexcept>

namespace reposco {
namespace {
class WipedBytes final {
  public:
    explicit WipedBytes(QByteArray value) : value_(std::move(value)) {
    }
    ~WipedBytes() {
        SecureZeroMemory(value_.data(), static_cast<SIZE_T>(value_.size()));
    }
    WipedBytes(const WipedBytes&) = delete;
    WipedBytes& operator=(const WipedBytes&) = delete;
    [[nodiscard]] const QByteArray& value() const {
        return value_;
    }

  private:
    QByteArray value_;
};
const QByteArray kEnvelopeHeader("RPKY\x01\0\0\0", 8);
} // namespace

DatabaseKey::~DatabaseKey() {
    SecureZeroMemory(bytes_.data(), bytes_.size());
}
std::span<const unsigned char> DatabaseKey::bytes() const {
    return bytes_;
}
std::unique_ptr<DatabaseKey> loadDatabaseKey(const QString& directory, bool historyExists) {
    auto key = std::make_unique<DatabaseKey>();
    const auto path = directory + "/key.dat";
    if (QFileInfo::exists(path)) {
        QFile envelope(path);
        if (!envelope.open(QIODevice::ReadOnly) || envelope.size() < 9 || envelope.size() > 65536) {
            throw std::runtime_error("Protected key could not be read");
        }
        const auto encoded = envelope.readAll();
        if (envelope.error() != QFileDevice::NoError || !encoded.startsWith(kEnvelopeHeader)) {
            throw std::runtime_error("Protected key envelope is invalid or unsupported");
        }
        auto unwrapped = unprotectKey(encoded.mid(kEnvelopeHeader.size()));
        if (!unwrapped) {
            throw std::runtime_error("Protected key could not be unlocked for this Windows user");
        }
        const WipedBytes plaintext(std::move(*unwrapped));
        if (plaintext.value().size() != static_cast<qsizetype>(key->bytes_.size())) {
            throw std::runtime_error("Protected key has an invalid size");
        }
        std::copy_n(reinterpret_cast<const unsigned char*>(plaintext.value().constData()),
                    key->bytes_.size(), key->bytes_.begin());
        return key;
    }
    if (historyExists) {
        throw std::runtime_error("Existing history is locked because its protected key is missing");
    }
    if (BCryptGenRandom(nullptr, key->bytes_.data(), static_cast<ULONG>(key->bytes_.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        throw std::runtime_error("A secure database key could not be generated");
    }
    const WipedBytes plaintext(QByteArray(reinterpret_cast<const char*>(key->bytes_.data()),
                                          static_cast<qsizetype>(key->bytes_.size())));
    const auto wrapped = protectKey(plaintext.value());
    if (!wrapped) {
        throw std::runtime_error("Database key could not be protected");
    }
    QSaveFile envelope(path);
    envelope.setDirectWriteFallback(false);
    const auto encoded = kEnvelopeHeader + *wrapped;
    if (!envelope.open(QIODevice::WriteOnly) || envelope.write(encoded) != encoded.size() ||
        !envelope.commit()) {
        throw std::runtime_error("Protected key could not be persisted atomically");
    }
    return key;
}
} // namespace reposco
