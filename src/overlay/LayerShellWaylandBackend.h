#pragma once

#include "WaylandOverlayBackend.h"

namespace LayerShellQt { class Window; }

class LayerShellWaylandBackend final : public WaylandOverlayBackend {
public:
    bool supportsCurrentCompositor(QString* error) const override;
    bool configure(QWidget* overlay, QScreen* screen, QString* error) override;
    void setScreen(QScreen* screen) override;

private:
    QWidget* overlay_ = nullptr;
    LayerShellQt::Window* layerWindow_ = nullptr;
};
