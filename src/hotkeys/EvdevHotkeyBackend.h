#pragma once

#include "hotkeys/HotkeyBackend.h"

#include <atomic>
#include <set>
#include <thread>
#include <vector>

class EvdevHotkeyBackend final : public HotkeyBackend {
public:
    EvdevHotkeyBackend() = default;
    ~EvdevHotkeyBackend() override;

    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error) override;
    void stop() override;
    QString limitation() const override;

private:
    void eventLoop();
    bool modifiersMatch() const;

    std::atomic_bool running_ = false;
    std::thread thread_;
    std::vector<int> fds_;
    std::set<unsigned short> pressed_;
    unsigned short targetCode_ = 0;
    Qt::KeyboardModifiers targetModifiers_ = Qt::NoModifier;
    Callback callback_;
};
