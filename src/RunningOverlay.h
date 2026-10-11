#pragma once

#include <QWidget>

class QScreen;
class WaylandOverlayBackend;

class RunningOverlay final : public QWidget {
    Q_OBJECT

public:
    enum class Backend {
        Unsupported,
        X11,
        WlrLayerShell,
    };

    explicit RunningOverlay(QWidget* parent = nullptr);
    ~RunningOverlay() override;

    static Backend selectBackend(const QString& qtPlatform, bool x11ShapeAvailable,
                                 bool layerShellAvailable);
    static Backend currentBackend();
    static bool isPlatformSupported();
    static QString platformLimitation();

    void setOverlayEnabled(bool enabled);
    void setClickingActive(bool active);
    bool overlayEnabled() const;
    bool clickingActive() const;
    bool isOverlayRequested() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void updateVisibility();
    void positionOnScreen(QScreen* screen);
    bool configureLayerShell();
    void applyX11InputShape();

    bool enabled_ = false;
    bool active_ = false;
    Backend backend_ = Backend::Unsupported;
    WaylandOverlayBackend* waylandBackend_ = nullptr;
};
