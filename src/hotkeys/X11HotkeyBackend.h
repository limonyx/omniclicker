#pragma once

#include "hotkeys/HotkeyBackend.h"

#include <atomic>
#include <thread>

struct _XDisplay;
using Display = _XDisplay;
using Window = unsigned long;

class X11HotkeyBackend final : public HotkeyBackend {
public:
    X11HotkeyBackend() = default;
    ~X11HotkeyBackend() override;

    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, QString* error) override;
    void stop() override;
    QString limitation() const override;

private:
    void eventLoop();

    std::atomic_bool running_ = false;
    std::thread thread_;
    Display* display_ = nullptr;
    Window root_ = 0;
    int keycode_ = 0;
    unsigned int modifiers_ = 0;
    Callback callback_;
};
