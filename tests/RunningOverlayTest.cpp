#include "RunningOverlay.h"

#include <QTest>

#ifdef HAVE_X11_SHAPE
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#endif

class RunningOverlayTest final : public QObject {
    Q_OBJECT

private slots:
    void backendSelectionRequiresNativeCapabilities()
    {
        QVERIFY(RunningOverlay::selectBackend(QStringLiteral("xcb"), true, false)
                == RunningOverlay::Backend::X11);
        QVERIFY(RunningOverlay::selectBackend(QStringLiteral("xcb"), false, true)
                == RunningOverlay::Backend::Unsupported);
        QVERIFY(RunningOverlay::selectBackend(QStringLiteral("wayland"), false, true)
                == RunningOverlay::Backend::WlrLayerShell);
        QVERIFY(RunningOverlay::selectBackend(QStringLiteral("wayland"), true, false)
                == RunningOverlay::Backend::Unsupported);
        QVERIFY(RunningOverlay::selectBackend(QStringLiteral("offscreen"), true, true)
                == RunningOverlay::Backend::Unsupported);
    }

    void requiresBothSettingAndActiveClicker()
    {
        RunningOverlay overlay;
        QVERIFY(!overlay.overlayEnabled());
        QVERIFY(!overlay.clickingActive());
        QVERIFY(!overlay.isOverlayRequested());

        overlay.setClickingActive(true);
        QVERIFY(overlay.clickingActive());
        QVERIFY(!overlay.isOverlayRequested());

        overlay.setOverlayEnabled(true);
        QVERIFY(overlay.isOverlayRequested());

        overlay.setClickingActive(false);
        QVERIFY(!overlay.isOverlayRequested());

        overlay.setClickingActive(true);
        QVERIFY(overlay.isOverlayRequested());
        overlay.setOverlayEnabled(false);
        QVERIFY(!overlay.isOverlayRequested());
    }

    void repeatedStateUpdatesAreIdempotent()
    {
        RunningOverlay overlay;
        overlay.setOverlayEnabled(true);
        overlay.setOverlayEnabled(true);
        overlay.setClickingActive(true);
        overlay.setClickingActive(true);
        QVERIFY(overlay.isOverlayRequested());

        overlay.setClickingActive(false);
        overlay.setClickingActive(false);
        QVERIFY(!overlay.isOverlayRequested());
    }

    void nativeX11WindowHasEmptyInputShape()
    {
        if (!RunningOverlay::isPlatformSupported()) {
            QSKIP("This test requires the Qt X11 backend and X Shape extension.");
        }
#ifdef HAVE_X11_SHAPE
        RunningOverlay overlay;
        QVERIFY(!overlay.isVisible());
        overlay.setOverlayEnabled(true);
        QVERIFY(!overlay.isVisible());
        overlay.setClickingActive(true);
        QVERIFY(overlay.isVisible());
        QCoreApplication::processEvents();
        QVERIFY(!overlay.hasFocus());
        QCOMPARE(overlay.focusPolicy(), Qt::NoFocus);
        QVERIFY(overlay.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(overlay.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));

        Display* display = XOpenDisplay(nullptr);
        QVERIFY(display != nullptr);
        XWindowAttributes attributes {};
        QVERIFY(XGetWindowAttributes(display, static_cast<::Window>(overlay.winId()), &attributes));
        QCOMPARE(attributes.map_state, IsViewable);
        int rootX = 0;
        int rootY = 0;
        ::Window child = None;
        QVERIFY(XTranslateCoordinates(display, static_cast<::Window>(overlay.winId()),
                                      DefaultRootWindow(display), 0, 0, &rootX, &rootY, &child));
        QCOMPARE(rootX, overlay.x());
        QCOMPARE(rootY, overlay.y());
#ifdef HAVE_KWINDOWSYSTEM
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long remaining = 0;
        unsigned char* typeData = nullptr;
        const Atom windowType = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
        QVERIFY(XGetWindowProperty(display, static_cast<::Window>(overlay.winId()), windowType,
                                   0, 8, False, XA_ATOM, &actualType, &actualFormat,
                                   &itemCount, &remaining, &typeData) == Success);
        const Atom osdType = XInternAtom(display, "_KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY", False);
        QVERIFY(actualType == XA_ATOM);
        QCOMPARE(actualFormat, 32);
        QVERIFY(itemCount >= 1);
        QCOMPARE(reinterpret_cast<Atom*>(typeData)[0], osdType);
        if (typeData) {
            XFree(typeData);
        }
#endif
        int count = -1;
        int ordering = 0;
        XRectangle* rectangles = XShapeGetRectangles(display, static_cast<::Window>(overlay.winId()),
                                                      ShapeInput, &count, &ordering);
        QCOMPARE(count, 0);
        if (rectangles) {
            XFree(rectangles);
        }
        overlay.setClickingActive(false);
        QVERIFY(!overlay.isVisible());
        overlay.setClickingActive(true);
        QVERIFY(overlay.isVisible());
        overlay.setOverlayEnabled(false);
        QVERIFY(!overlay.isVisible());
        XCloseDisplay(display);
#endif
    }

    void unsupportedBackendNeverShowsAnInteractiveFallback()
    {
        if (RunningOverlay::isPlatformSupported()) {
            QSKIP("The active backend supports the X11 overlay.");
        }
        RunningOverlay overlay;
        overlay.setOverlayEnabled(true);
        overlay.setClickingActive(true);
        QVERIFY(overlay.isOverlayRequested());
        QVERIFY(!overlay.isVisible());
        QVERIFY(!RunningOverlay::platformLimitation().isEmpty());
    }

    void nativeWaylandLayerShellHasRequiredSurfaceSettings()
    {
#if defined(HAVE_WAYLAND_LAYER_SHELL) && QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (QGuiApplication::platformName() != QStringLiteral("wayland")) {
            QSKIP("This test requires the native Qt Wayland backend.");
        }
        if (RunningOverlay::currentBackend() != RunningOverlay::Backend::WlrLayerShell) {
            QSKIP("The compositor does not advertise zwlr_layer_shell_v1.");
        }

        RunningOverlay overlay;
        overlay.setOverlayEnabled(true);
        overlay.setClickingActive(true);
        QVERIFY(overlay.isVisible());
        QCoreApplication::processEvents();
        QVERIFY(overlay.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
        QVERIFY(overlay.windowFlags().testFlag(Qt::WindowTransparentForInput));

        overlay.setOverlayEnabled(false);
        QVERIFY(!overlay.isVisible());
        overlay.setOverlayEnabled(true);
        QVERIFY(overlay.isVisible());
#else
        QSKIP("Native Wayland layer-shell requires Qt 6.8 and the optional LayerShellQt integration.");
#endif
    }
};

QTEST_MAIN(RunningOverlayTest)
#include "RunningOverlayTest.moc"
