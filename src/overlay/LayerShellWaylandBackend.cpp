#include "LayerShellWaylandBackend.h"

#include <LayerShellQt/window.h>
#include <wayland-client.h>
#include <QtGui/qguiapplication_platform.h>

#include <cstring>

namespace {

struct WaylandGlobals {
    bool layerShell = false;
};

void globalAdded(void* data, wl_registry*, uint32_t, const char* interface, uint32_t)
{
    auto* globals = static_cast<WaylandGlobals*>(data);
    if (std::strcmp(interface, "zwlr_layer_shell_v1") == 0) {
        globals->layerShell = true;
    }
}

void globalRemoved(void*, wl_registry*, uint32_t) {}

bool compositorHasLayerShell(QString* error)
{
    // Query Qt's live Wayland connection. In particular, do not open a second
    // connection here: launchers and sandboxed sessions may expose Qt's
    // already-established connection without allowing clients to reconnect
    // through the default socket name.
    auto* waylandApplication = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    wl_display* display = waylandApplication ? waylandApplication->display() : nullptr;
    if (!display) {
        if (error) {
            *error = QStringLiteral("Qt did not expose its native Wayland display.");
        }
        return false;
    }
    wl_registry* registry = wl_display_get_registry(display);
    if (!registry) {
        if (error) {
            *error = QStringLiteral("Could not query the registry on Qt's Wayland display.");
        }
        return false;
    }

    const wl_registry_listener listener = {globalAdded, globalRemoved};
    WaylandGlobals globals;
    wl_registry_add_listener(registry, &listener, &globals);
    const bool connected = wl_display_roundtrip(display) >= 0;
    wl_registry_destroy(registry);
    if (!connected) {
        if (error) {
            *error = QStringLiteral("The Wayland registry query on Qt's display failed.");
        }
        return false;
    }
    if (!globals.layerShell && error) {
        *error = QStringLiteral("The compositor does not advertise zwlr_layer_shell_v1.");
    }
    return globals.layerShell;
}

} // namespace

bool LayerShellWaylandBackend::supportsCurrentCompositor(QString* error) const
{
    if (!compositorHasLayerShell(error)) {
        return false;
    }
    return true;
}

bool LayerShellWaylandBackend::configure(QWidget* overlay, QScreen* screen, QString* error)
{
    if (!overlay || !screen) {
        if (error) {
            *error = QStringLiteral("The layer-shell overlay needs a widget and an output.");
        }
        return false;
    }

    // Create the Qt surface first, then replace its shell role before it is shown.
    overlay->winId();
    QWindow* nativeWindow = overlay->windowHandle();
    if (!nativeWindow) {
        if (error) {
            *error = QStringLiteral("Qt could not create the Wayland overlay surface.");
        }
        return false;
    }

    layerWindow_ = LayerShellQt::Window::get(nativeWindow);
    if (!layerWindow_) {
        if (error) {
            *error = QStringLiteral("LayerShellQt could not configure the Wayland surface.");
        }
        return false;
    }

    overlay_ = overlay;
    // The top layer sits below fullscreen shell surfaces; the protocol's
    // overlay layer is intended for OSDs that must remain visible over them.
    layerWindow_->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow_->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorBottom)
                             | LayerShellQt::Window::AnchorRight);
    layerWindow_->setMargins(QMargins(0, 0, 18, 18));
    layerWindow_->setExclusiveZone(-1);
    layerWindow_->setDesiredSize(overlay->size());
    layerWindow_->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow_->setActivateOnShow(false);
    layerWindow_->setCloseOnDismissed(false);
    setScreen(screen);

    // QtWayland 6.8+ maps this flag to an empty wl_surface input region.
    nativeWindow->setMask(QRegion());
    return true;
}

void LayerShellWaylandBackend::setScreen(QScreen* screen)
{
    if (!screen) {
        return;
    }
    if (layerWindow_) {
        layerWindow_->setScreen(screen);
    }
    if (overlay_ && overlay_->windowHandle()) {
        overlay_->windowHandle()->setScreen(screen);
    }
}
