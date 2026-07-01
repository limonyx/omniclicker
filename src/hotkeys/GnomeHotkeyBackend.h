#pragma once

#include "hotkeys/HotkeyBackend.h"

class GnomeHotkeyBackend final : public HotkeyBackend {
public:
    GnomeHotkeyBackend() = default;
    ~GnomeHotkeyBackend() override;

    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, QString* error) override;
    void stop() override;
    QString limitation() const override;

private:
    void removeFromGsettings();

    bool active_ = false;
    Callback callback_;
};
