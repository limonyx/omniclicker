#pragma once

#include "hotkeys/HotkeyBackend.h"
#include <QString>

class SwayHotkeyBackend final : public HotkeyBackend {
public:
    SwayHotkeyBackend() = default;
    ~SwayHotkeyBackend() override;

    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, QString* error) override;
    void stop() override;
    QString limitation() const override;

private:
    bool active_ = false;
    Callback callback_;
    QString boundKey_; // Stores the sway keybind string so we can unbind it
};
