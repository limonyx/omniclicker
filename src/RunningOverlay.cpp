#include "RunningOverlay.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
#include <QShowEvent>
#include <QWindow>

#ifdef HAVE_KWINDOWSYSTEM
#include <KX11Extras>
#include <NETWM>
#endif

#include "overlay/WaylandOverlayBackend.h"
#include "overlay/LayerShellWaylandBackend.h"

#ifdef HAVE_X11_SHAPE
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#endif

namespace {
constexpr int OverlayWidth = 190;
constexpr int OverlayHeight = 40;
constexpr int ScreenMargin = 18;

#ifdef HAVE_X11_SHAPE
bool x11ShapeAvailable()
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        return false;
    }
    int eventBase = 0;
    int errorBase = 0;
    const bool available = XShapeQueryExtension(display, &eventBase, &errorBase);
    XCloseDisplay(display);
    return available;
}
#endif

#ifdef HAVE_WAYLAND_LAYER_SHELL
bool waylandLayerShellSupportsCurrentCompositor()
{
    static const bool supported = []() {
        LayerShellWaylandBackend backend;
        QString error;
        const bool result = backend.supportsCurrentCompositor(&error);
        if (!result) {
            qWarning("OmniClicker Wayland overlay backend unavailable: %s",
                     qPrintable(error));
        }
        return result;
    }();
    return supported;
}
#endif
}

RunningOverlay::RunningOverlay(QWidget* parent)
    : QWidget(parent)
    , backend_(currentBackend())
{
    setObjectName(QStringLiteral("RunningStateOverlay"));
    Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                            | Qt::WindowDoesNotAcceptFocus;
    // X11 gets a server-side empty ShapeInput region below. Qt's generic flag
    // is retained for Wayland, where it maps to wl_surface input-region state.
    if (backend_ != Backend::X11) {
        flags |= Qt::WindowTransparentForInput;
    }
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(OverlayWidth, OverlayHeight);

#ifdef HAVE_WAYLAND_LAYER_SHELL
    if (backend_ == Backend::WlrLayerShell) {
        waylandBackend_ = new LayerShellWaylandBackend;
    }
#endif

    for (QScreen* screen : QGuiApplication::screens()) {
        connect(screen, &QScreen::geometryChanged, this, [this, screen]() {
            if (isVisible() && screen == QGuiApplication::primaryScreen()) {
                positionOnScreen(screen);
            }
        });
    }
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen* screen) {
        connect(screen, &QScreen::geometryChanged, this, [this, screen]() {
            if (isVisible() && screen == QGuiApplication::primaryScreen()) {
                positionOnScreen(screen);
            }
        });
    });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this](QScreen* screen) {
        if (isVisible()) {
            if (backend_ == Backend::X11) {
                positionOnScreen(screen);
            } else if (backend_ == Backend::WlrLayerShell) {
                if (waylandBackend_) {
                    waylandBackend_->setScreen(screen);
                    hide();
                    show();
                }
            }
        }
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) {
        if (isVisible() && backend_ == Backend::WlrLayerShell) {
            if (waylandBackend_) {
                waylandBackend_->setScreen(QGuiApplication::primaryScreen());
                hide();
                show();
            }
        }
    });
}

RunningOverlay::~RunningOverlay()
{
    delete waylandBackend_;
    waylandBackend_ = nullptr;
}

RunningOverlay::Backend RunningOverlay::selectBackend(const QString& qtPlatform,
                                                       bool x11ShapeAvailable,
                                                       bool layerShellAvailable)
{
    if (qtPlatform == QStringLiteral("xcb") && x11ShapeAvailable) {
        return Backend::X11;
    }
    if (qtPlatform == QStringLiteral("wayland") && layerShellAvailable) {
        return Backend::WlrLayerShell;
    }
    return Backend::Unsupported;
}

RunningOverlay::Backend RunningOverlay::currentBackend()
{
    const QString platform = QGuiApplication::platformName();
    bool x11Available = false;
    bool layerShellAvailable = false;
#ifdef HAVE_X11_SHAPE
    if (platform == QStringLiteral("xcb")) {
        static const bool available = x11ShapeAvailable();
        x11Available = available;
    }
#endif
#ifdef HAVE_WAYLAND_LAYER_SHELL
    if (platform == QStringLiteral("wayland")) {
        static const bool available = waylandLayerShellSupportsCurrentCompositor();
        layerShellAvailable = available;
    }
#endif
    return selectBackend(platform, x11Available, layerShellAvailable);
}

bool RunningOverlay::isPlatformSupported()
{
    return currentBackend() != Backend::Unsupported;
}

QString RunningOverlay::platformLimitation()
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("wayland")) {
        if (currentBackend() == Backend::WlrLayerShell) {
            return QString();
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
        return QStringLiteral("The running overlay requires Qt 6.8 or newer for Wayland input pass-through.");
#elif !defined(HAVE_WAYLAND_LAYER_SHELL)
        return QStringLiteral("The optional Wayland layer-shell backend was not built. Install LayerShellQt and Wayland client development files, then rebuild OmniClicker.");
#else
        return QStringLiteral("The compositor does not advertise zwlr_layer_shell_v1; no interactive fallback is shown.");
#endif
    }
    if (platform == QStringLiteral("xcb")) {
#ifdef HAVE_X11_SHAPE
        if (isPlatformSupported()) {
            return QString();
        }
        return QStringLiteral("The running overlay is unavailable because the X11 Shape extension is not available on this display.");
#else
        return QStringLiteral("The running overlay is unavailable because this build lacks the X11 Shape extension.");
#endif
    }
    return QStringLiteral("The running overlay is unavailable on the %1 display backend.").arg(platform);
}

void RunningOverlay::setOverlayEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    updateVisibility();
}

void RunningOverlay::setClickingActive(bool active)
{
    if (active_ == active) {
        return;
    }
    active_ = active;
    updateVisibility();
}

bool RunningOverlay::overlayEnabled() const { return enabled_; }
bool RunningOverlay::clickingActive() const { return active_; }
bool RunningOverlay::isOverlayRequested() const { return enabled_ && active_; }

void RunningOverlay::updateVisibility()
{
    if (!enabled_ || !active_ || backend_ == Backend::Unsupported) {
#ifdef HAVE_KWINDOWSYSTEM
        if (backend_ == Backend::X11 && isVisible()) {
            // Keep KWin's OSD placement policy from recentering this window
            // the next time it is mapped. Restore the OSD layer after mapping.
            KX11Extras::setType(static_cast<WId>(winId()), NET::Utility);
        }
#endif
        hide();
        return;
    }
    if (backend_ == Backend::X11) {
        positionOnScreen(QGuiApplication::primaryScreen());
    } else if (!configureLayerShell()) {
        backend_ = Backend::Unsupported;
        hide();
        return;
    }
    if (!isVisible()) {
        show();
    }
    raise();
    if (backend_ == Backend::X11) {
        applyX11InputShape();
#ifdef HAVE_KWINDOWSYSTEM
        // Set this after mapping: KWin places OSD windows above fullscreen,
        // but applies its own centered OSD placement when that type is present
        // at initial map time.
        KX11Extras::setType(static_cast<WId>(winId()), NET::OnScreenDisplay);
#endif
    }
}

void RunningOverlay::positionOnScreen(QScreen* screen)
{
    if (!screen) {
        return;
    }
    const QRect available = screen->availableGeometry();
    move(available.right() - width() - ScreenMargin + 1,
         available.bottom() - height() - ScreenMargin + 1);
}

void RunningOverlay::applyX11InputShape()
{
#ifdef HAVE_X11_SHAPE
    if (!isVisible() || backend_ != Backend::X11) {
        return;
    }
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        return;
    }
    int eventBase = 0;
    int errorBase = 0;
    if (XShapeQueryExtension(display, &eventBase, &errorBase)) {
        const ::Window overlayWindow = static_cast<::Window>(winId());
        XShapeCombineRectangles(display, overlayWindow, ShapeInput,
                                0, 0, nullptr, 0, ShapeSet, Unsorted);
        // Re-assert stacking after the empty input shape is installed. Some
        // X11 window managers restack tool windows while processing shape
        // changes made immediately after mapping.
        XRaiseWindow(display, overlayWindow);
        XSync(display, False);
    }
    XCloseDisplay(display);
#endif
}

bool RunningOverlay::configureLayerShell()
{
#ifdef HAVE_WAYLAND_LAYER_SHELL
    if (backend_ != Backend::WlrLayerShell || !waylandBackend_) {
        return false;
    }
    QString error;
    if (!waylandBackend_->configure(this, QGuiApplication::primaryScreen(), &error)) {
        qWarning("OmniClicker layer-shell setup failed: %s", qPrintable(error));
        return false;
    }
    return true;
#else
    return false;
#endif
}

void RunningOverlay::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    applyX11InputShape();
}

void RunningOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 23, 28, 218));
    painter.drawRoundedRect(rect(), 10, 10);
    painter.setBrush(QColor(68, 220, 120));
    painter.drawEllipse(QRect(15, 15, 10, 10));
    painter.setPen(QColor(245, 247, 249));
    QFont font = painter.font();
    font.setPixelSize(12);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(34, 0, width() - 45, height()), Qt::AlignVCenter,
                     QStringLiteral("AUTOCLICKER ACTIVE"));
}
