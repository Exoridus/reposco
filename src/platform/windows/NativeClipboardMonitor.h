#pragma once

#include "platform/interfaces/ClipboardMonitor.h"
#include <memory>

namespace reposco {
class NativeClipboardMonitor final : public ClipboardMonitor {
    Q_OBJECT
  public:
    explicit NativeClipboardMonitor(QObject* parent = nullptr);
    ~NativeClipboardMonitor() override;
    void start() override;
    void setActive(bool active, quint64 generation) override;
    void acknowledge(quint64 token) override;
    void stop() override;

  private:
    class Thread;
    std::unique_ptr<Thread> thread_;
};
} // namespace reposco
