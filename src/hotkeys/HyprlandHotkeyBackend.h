#pragma once

#include "hotkeys/HotkeyBackend.h"
#include <QString>

class HyprlandHotkeyBackend final : public HotkeyBackend {
public:
    HyprlandHotkeyBackend() = default;
    ~HyprlandHotkeyBackend() override;

    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error) override;
    void stop() override;
    QString limitation() const override;

private:
    bool active_ = false;
    Callback callback_;
    QString boundModifiers_; // Stores the modifier string for unbind
    QString boundKey_;       // Stores the key string for unbind
};
