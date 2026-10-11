#pragma once

#include <QScreen>
#include <QString>
#include <QWidget>

class WaylandOverlayBackend {
public:
    virtual ~WaylandOverlayBackend() = default;
    virtual bool supportsCurrentCompositor(QString* error) const = 0;
    virtual bool configure(QWidget* overlay, QScreen* screen, QString* error) = 0;
    virtual void setScreen(QScreen* screen) = 0;
};

#define WaylandOverlayBackend_iid "io.github.omniclicker.WaylandOverlayBackend/1.0"
Q_DECLARE_INTERFACE(WaylandOverlayBackend, WaylandOverlayBackend_iid)
