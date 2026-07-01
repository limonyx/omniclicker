#include "hotkeys/X11HotkeyBackend.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <chrono>
#include <thread>

namespace {

int lastGrabError = 0;

int grabErrorHandler(Display*, XErrorEvent* event)
{
    lastGrabError = event->error_code;
    return 0;
}

KeySym qtKeyToKeySym(int key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return XK_F1 + key - Qt::Key_F1;
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return XK_A + key - Qt::Key_A;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return XK_0 + key - Qt::Key_0;
    }

    switch (key) {
    case Qt::Key_Space:
        return XK_space;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return XK_Tab;
    case Qt::Key_Insert:
        return XK_Insert;
    case Qt::Key_Delete:
        return XK_Delete;
    case Qt::Key_Backspace:
        return XK_BackSpace;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return XK_Return;
    case Qt::Key_Escape:
        return XK_Escape;
    case Qt::Key_Home:
        return XK_Home;
    case Qt::Key_End:
        return XK_End;
    case Qt::Key_PageUp:
        return XK_Page_Up;
    case Qt::Key_PageDown:
        return XK_Page_Down;
    case Qt::Key_Left:
        return XK_Left;
    case Qt::Key_Right:
        return XK_Right;
    case Qt::Key_Up:
        return XK_Up;
    case Qt::Key_Down:
        return XK_Down;
    case Qt::Key_Slash:
        return XK_slash;
    case Qt::Key_Backslash:
        return XK_backslash;
    case Qt::Key_Minus:
        return XK_minus;
    case Qt::Key_Equal:
        return XK_equal;
    case Qt::Key_Comma:
        return XK_comma;
    case Qt::Key_Period:
        return XK_period;
    case Qt::Key_Semicolon:
        return XK_semicolon;
    case Qt::Key_Apostrophe:
        return XK_apostrophe;
    case Qt::Key_BracketLeft:
        return XK_bracketleft;
    case Qt::Key_BracketRight:
        return XK_bracketright;
    case Qt::Key_QuoteLeft:
        return XK_grave;
    default:
        return NoSymbol;
    }
}

unsigned int x11Modifiers(Qt::KeyboardModifiers modifiers)
{
    unsigned int result = 0;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        result |= ControlMask;
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        result |= Mod1Mask;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        result |= ShiftMask;
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        result |= Mod4Mask;
    }
    return result;
}

} // namespace

X11HotkeyBackend::~X11HotkeyBackend()
{
    stop();
}

QString X11HotkeyBackend::name() const
{
    return QStringLiteral("X11 passive key grab");
}

bool X11HotkeyBackend::start(const Hotkey& hotkey, Callback callback, QString* error)
{
    stop();

    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        if (error) {
            *error = QStringLiteral("Cannot open X11 display for global hotkeys.");
        }
        return false;
    }

    const KeySym keysym = qtKeyToKeySym(hotkey.key);
    if (keysym == NoSymbol) {
        if (error) {
            *error = QStringLiteral("Unsupported hotkey key for X11 backend.");
        }
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    keycode_ = XKeysymToKeycode(display_, keysym);
    if (keycode_ == 0) {
        if (error) {
            *error = QStringLiteral("Could not resolve hotkey keycode on this X11 layout.");
        }
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    root_ = DefaultRootWindow(display_);
    modifiers_ = x11Modifiers(hotkey.modifiers);
    callback_ = std::move(callback);

    const unsigned int ignoredLocks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
    lastGrabError = 0;
    auto previousHandler = XSetErrorHandler(grabErrorHandler);
    for (unsigned int ignored : ignoredLocks) {
        XGrabKey(display_, keycode_, modifiers_ | ignored, root_, False, GrabModeAsync, GrabModeAsync);
    }
    XSync(display_, False);
    XSetErrorHandler(previousHandler);

    if (lastGrabError != 0) {
        if (error) {
            *error = lastGrabError == BadAccess
                ? QStringLiteral("Another application already owns this global shortcut.")
                : QStringLiteral("X11 failed to register the global shortcut. X error code: %1").arg(lastGrabError);
        }
        for (unsigned int ignored : ignoredLocks) {
            XUngrabKey(display_, keycode_, modifiers_ | ignored, root_);
        }
        XSync(display_, False);
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    running_ = true;
    thread_ = std::thread(&X11HotkeyBackend::eventLoop, this);
    return true;
}

void X11HotkeyBackend::stop()
{
    if (!running_.exchange(false)) {
        return;
    }

    if (thread_.joinable()) {
        thread_.join();
    }
}

QString X11HotkeyBackend::limitation() const
{
    const bool isWayland = !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    if (isWayland) {
        return QStringLiteral(
            "⚠ Running via XWayland fallback. The GNOME global shortcuts portal has a known bug "
            "that prevents shortcut registration. Hotkeys work through XWayland but may not be "
            "captured when a native Wayland window is focused. Modifier+key combos (e.g. Ctrl+F9) "
            "are more reliable than bare keys.");
    }
    return QStringLiteral("X11 global hotkeys use passive key grabs. Another application may already own the same shortcut.");
}

void X11HotkeyBackend::eventLoop()
{
    auto lastActivation = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    while (running_.load()) {
        while (display_ && XPending(display_) > 0) {
            XEvent event {};
            XNextEvent(display_, &event);

            if (event.type != KeyPress || event.xkey.keycode != static_cast<unsigned int>(keycode_)) {
                continue;
            }

            const unsigned int activeModifiers = event.xkey.state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
            if (activeModifiers != modifiers_) {
                continue;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - lastActivation > std::chrono::milliseconds(250)) {
                lastActivation = now;
                if (callback_) {
                    callback_();
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (display_) {
        const unsigned int ignoredLocks[] = { 0, LockMask, Mod2Mask, LockMask | Mod2Mask };
        for (unsigned int ignored : ignoredLocks) {
            XUngrabKey(display_, keycode_, modifiers_ | ignored, root_);
        }
        XSync(display_, False);
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}
