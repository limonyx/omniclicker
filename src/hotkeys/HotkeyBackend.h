#pragma once

#include "hotkeys/Hotkey.h"

#include <functional>
#include <memory>
#include <vector>

class HotkeyBackend {
public:
    using Callback = std::function<void()>;

    virtual ~HotkeyBackend() = default;

    virtual QString name() const = 0;
    virtual bool start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error) = 0;
    virtual void stop() = 0;
    virtual QString limitation() const = 0;
};

std::unique_ptr<HotkeyBackend> createHotkeyBackend();
std::vector<std::unique_ptr<HotkeyBackend>> createHotkeyBackendCandidates();
