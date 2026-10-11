#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

namespace reposco {
class GlobalHotkey final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
  public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;
    [[nodiscard]] bool registerDefault();
    void unregister();
    [[nodiscard]] unsigned long errorCode() const;
    [[nodiscard]] bool registerShortcut(unsigned int modifiers, unsigned int key);
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
  signals:
    void activated();

  private:
    bool registered_ = false;
    unsigned long error_code_ = 0;
};
} // namespace reposco
