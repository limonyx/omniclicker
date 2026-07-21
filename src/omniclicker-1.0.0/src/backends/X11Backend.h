#pragma once

#include "backends/InputBackend.h"

struct _XDisplay;
using Display = _XDisplay;
using Window = unsigned long;

class X11Backend final : public InputBackend {
public:
    X11Backend() = default;
    ~X11Backend() override;

    QString name() const override;
    bool initialize(QString* error) override;
    BackendCapabilities capabilities() const override;
    bool click(const ClickSettings& settings, QString* error) override;
    std::optional<QPoint> currentPosition(QString* error) override;

private:
    Display* display_ = nullptr;
    Window root_ = 0;
};
