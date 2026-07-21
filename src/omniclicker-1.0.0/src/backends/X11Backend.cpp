#include "backends/X11Backend.h"

#include "hotkeys/Hotkey.h"

#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <chrono>
#include <thread>
#include <vector>

namespace {

int x11Button(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return 1;
    case MouseButton::Middle:
        return 2;
    case MouseButton::Right:
        return 3;
    case MouseButton::CustomKey:
        return 1;
    }

    return 1;
}

KeySym qtKeyToKeySym(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return XK_A + key - Qt::Key_A;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return XK_0 + key - Qt::Key_0;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return XK_F1 + key - Qt::Key_F1;
    }

    switch (key) {
    case Qt::Key_Space:
        return XK_space;
    case Qt::Key_Tab:
        return XK_Tab;
    case Qt::Key_Backtab:
        return XK_ISO_Left_Tab;
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

std::vector<KeySym> modifierKeySyms(Qt::KeyboardModifiers modifiers)
{
    std::vector<KeySym> keys;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        keys.push_back(XK_Control_L);
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        keys.push_back(XK_Alt_L);
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        keys.push_back(XK_Shift_L);
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        keys.push_back(XK_Super_L);
    }
    return keys;
}

bool fakeKey(Display* display, KeySym sym, bool press, QString* error)
{
    const KeyCode code = XKeysymToKeycode(display, sym);
    if (code == 0) {
        if (error) {
            *error = QStringLiteral("The selected key is not available in the active X11 keyboard map.");
        }
        return false;
    }

    if (!XTestFakeKeyEvent(display, code, press ? True : False, CurrentTime)) {
        if (error) {
            *error = QStringLiteral("XTest failed to send key event.");
        }
        return false;
    }
    return true;
}

} // namespace

X11Backend::~X11Backend()
{
    if (display_) {
        XCloseDisplay(display_);
    }
}

QString X11Backend::name() const
{
    return QStringLiteral("X11/XTest");
}

bool X11Backend::initialize(QString* error)
{
    if (display_) {
        return true;
    }

    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        if (error) {
            *error = QStringLiteral("Cannot open X11 display. Check DISPLAY and Xauthority.");
        }
        return false;
    }

    int eventBase = 0;
    int errorBase = 0;
    int major = 0;
    int minor = 0;
    if (!XTestQueryExtension(display_, &eventBase, &errorBase, &major, &minor)) {
        if (error) {
            *error = QStringLiteral("The XTest extension is not available on this X11 server.");
        }
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;
    }

    root_ = DefaultRootWindow(display_);
    return true;
}

BackendCapabilities X11Backend::capabilities() const
{
    BackendCapabilities caps;
    caps.supportsCurrentPositionClicking = true;
    caps.supportsFixedPositionClicking = true;
    caps.supportsCursorCapture = true;
    return caps;
}

bool X11Backend::click(const ClickSettings& settings, QString* error)
{
    if (!display_ && !initialize(error)) {
        return false;
    }

    if (settings.mouseButton == MouseButton::CustomKey) {
        const std::optional<Hotkey> key = parseHotkey(settings.customKey, error);
        if (!key) {
            return false;
        }

        const KeySym keySym = qtKeyToKeySym(key->key);
        if (keySym == NoSymbol) {
            if (error) {
                *error = QStringLiteral("Unsupported custom key for X11 input.");
            }
            return false;
        }

        const std::vector<KeySym> modifiers = modifierKeySyms(key->modifiers);
        const int repetitions = settings.clickType == ClickType::Double ? 2 : 1;
        for (int i = 0; i < repetitions; ++i) {
            for (KeySym modifier : modifiers) {
                if (!fakeKey(display_, modifier, true, error)) {
                    return false;
                }
            }
            if (!fakeKey(display_, keySym, true, error) || !fakeKey(display_, keySym, false, error)) {
                return false;
            }
            for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
                if (!fakeKey(display_, *it, false, error)) {
                    return false;
                }
            }
            XFlush(display_);

            if (settings.clickType == ClickType::Double && i == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
            }
        }
        return true;
    }

    if (settings.positionMode == PositionMode::FixedCoordinate) {
        XWarpPointer(display_, None, root_, 0, 0, 0, 0, settings.fixedPosition.x(), settings.fixedPosition.y());
        XFlush(display_);
    }

    const int button = x11Button(settings.mouseButton);
    const int repetitions = settings.clickType == ClickType::Double ? 2 : 1;

    for (int i = 0; i < repetitions; ++i) {
        if (!XTestFakeButtonEvent(display_, button, True, CurrentTime)) {
            if (error) {
                *error = QStringLiteral("XTest failed to send button press.");
            }
            return false;
        }

        if (!XTestFakeButtonEvent(display_, button, False, CurrentTime)) {
            if (error) {
                *error = QStringLiteral("XTest failed to send button release.");
            }
            return false;
        }

        XFlush(display_);

        if (settings.clickType == ClickType::Double && i == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(35));
        }
    }

    return true;
}

std::optional<QPoint> X11Backend::currentPosition(QString* error)
{
    if (!display_ && !initialize(error)) {
        return std::nullopt;
    }

    Window rootReturn = 0;
    Window childReturn = 0;
    int rootX = 0;
    int rootY = 0;
    int windowX = 0;
    int windowY = 0;
    unsigned int mask = 0;

    if (!XQueryPointer(display_, root_, &rootReturn, &childReturn, &rootX, &rootY, &windowX, &windowY, &mask)) {
        if (error) {
            *error = QStringLiteral("Failed to query the X11 pointer position.");
        }
        return std::nullopt;
    }

    return QPoint(rootX, rootY);
}
