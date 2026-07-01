#include "backends/UInputBackend.h"

#include "hotkeys/Hotkey.h"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

namespace {

int uinputButton(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return BTN_LEFT;
    case MouseButton::Middle:
        return BTN_MIDDLE;
    case MouseButton::Right:
        return BTN_RIGHT;
    case MouseButton::CustomKey:
        return BTN_LEFT;
    }

    return BTN_LEFT;
}

std::optional<int> qtKeyToUInputCode(int key)
{
    switch (key) {
    case Qt::Key_A:
        return KEY_A;
    case Qt::Key_B:
        return KEY_B;
    case Qt::Key_C:
        return KEY_C;
    case Qt::Key_D:
        return KEY_D;
    case Qt::Key_E:
        return KEY_E;
    case Qt::Key_F:
        return KEY_F;
    case Qt::Key_G:
        return KEY_G;
    case Qt::Key_H:
        return KEY_H;
    case Qt::Key_I:
        return KEY_I;
    case Qt::Key_J:
        return KEY_J;
    case Qt::Key_K:
        return KEY_K;
    case Qt::Key_L:
        return KEY_L;
    case Qt::Key_M:
        return KEY_M;
    case Qt::Key_N:
        return KEY_N;
    case Qt::Key_O:
        return KEY_O;
    case Qt::Key_P:
        return KEY_P;
    case Qt::Key_Q:
        return KEY_Q;
    case Qt::Key_R:
        return KEY_R;
    case Qt::Key_S:
        return KEY_S;
    case Qt::Key_T:
        return KEY_T;
    case Qt::Key_U:
        return KEY_U;
    case Qt::Key_V:
        return KEY_V;
    case Qt::Key_W:
        return KEY_W;
    case Qt::Key_X:
        return KEY_X;
    case Qt::Key_Y:
        return KEY_Y;
    case Qt::Key_Z:
        return KEY_Z;
    default:
        break;
    }

    if (key >= Qt::Key_1 && key <= Qt::Key_9) {
        return KEY_1 + key - Qt::Key_1;
    }
    if (key == Qt::Key_0) {
        return KEY_0;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return KEY_F1 + key - Qt::Key_F1;
    }

    switch (key) {
    case Qt::Key_Space:
        return KEY_SPACE;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return KEY_TAB;
    case Qt::Key_Insert:
        return KEY_INSERT;
    case Qt::Key_Delete:
        return KEY_DELETE;
    case Qt::Key_Backspace:
        return KEY_BACKSPACE;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return KEY_ENTER;
    case Qt::Key_Escape:
        return KEY_ESC;
    case Qt::Key_Home:
        return KEY_HOME;
    case Qt::Key_End:
        return KEY_END;
    case Qt::Key_PageUp:
        return KEY_PAGEUP;
    case Qt::Key_PageDown:
        return KEY_PAGEDOWN;
    case Qt::Key_Left:
        return KEY_LEFT;
    case Qt::Key_Right:
        return KEY_RIGHT;
    case Qt::Key_Up:
        return KEY_UP;
    case Qt::Key_Down:
        return KEY_DOWN;
    case Qt::Key_Slash:
        return KEY_SLASH;
    case Qt::Key_Backslash:
        return KEY_BACKSLASH;
    case Qt::Key_Minus:
        return KEY_MINUS;
    case Qt::Key_Equal:
        return KEY_EQUAL;
    case Qt::Key_Comma:
        return KEY_COMMA;
    case Qt::Key_Period:
        return KEY_DOT;
    case Qt::Key_Semicolon:
        return KEY_SEMICOLON;
    case Qt::Key_Apostrophe:
        return KEY_APOSTROPHE;
    case Qt::Key_BracketLeft:
        return KEY_LEFTBRACE;
    case Qt::Key_BracketRight:
        return KEY_RIGHTBRACE;
    case Qt::Key_QuoteLeft:
        return KEY_GRAVE;
    default:
        return std::nullopt;
    }
}

std::vector<int> modifierCodes(Qt::KeyboardModifiers modifiers)
{
    std::vector<int> codes;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        codes.push_back(KEY_LEFTCTRL);
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        codes.push_back(KEY_LEFTALT);
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        codes.push_back(KEY_LEFTSHIFT);
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        codes.push_back(KEY_LEFTMETA);
    }
    return codes;
}

QString errnoMessage(const QString& context)
{
    return QStringLiteral("%1: %2").arg(context, QString::fromLocal8Bit(std::strerror(errno)));
}

} // namespace

UInputBackend::~UInputBackend()
{
    if (fd_ >= 0) {
        ioctl(fd_, UI_DEV_DESTROY);
        close(fd_);
    }
}

QString UInputBackend::name() const
{
    return QStringLiteral("Wayland/uinput virtual input");
}

bool UInputBackend::initialize(QString* error)
{
    if (fd_ >= 0) {
        return true;
    }

    fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) {
        if (error) {
            *error = errnoMessage(QStringLiteral("Cannot open /dev/uinput. Load the uinput module and grant write permission"));
        }
        return false;
    }

    if (ioctl(fd_, UI_SET_EVBIT, EV_KEY) < 0
        || ioctl(fd_, UI_SET_EVBIT, EV_REL) < 0
        || ioctl(fd_, UI_SET_RELBIT, REL_X) < 0
        || ioctl(fd_, UI_SET_RELBIT, REL_Y) < 0) {
        if (error) {
            *error = errnoMessage(QStringLiteral("Failed to configure uinput virtual input device"));
        }
        close(fd_);
        fd_ = -1;
        return false;
    }

    for (int code = 1; code < KEY_MAX; ++code) {
        ioctl(fd_, UI_SET_KEYBIT, code);
    }

    uinput_setup setup {};
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1d6b;
    setup.id.product = 0x0104;
    std::snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "Omni Clicker Virtual Input");

    if (ioctl(fd_, UI_DEV_SETUP, &setup) < 0 || ioctl(fd_, UI_DEV_CREATE) < 0) {
        if (error) {
            *error = errnoMessage(QStringLiteral("Failed to create uinput virtual input device"));
        }
        close(fd_);
        fd_ = -1;
        return false;
    }

    // Give the compositor a short window to discover the virtual input device.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

BackendCapabilities UInputBackend::capabilities() const
{
    BackendCapabilities caps;
    caps.supportsCurrentPositionClicking = true;
    caps.supportsFixedPositionClicking = false;
    caps.supportsCursorCapture = false;
    caps.limitations << QStringLiteral("You are running Wayland. For security, Linux desktops usually do not let apps freely read or move your mouse pointer.")
                     << QStringLiteral("What works here: clicking wherever your mouse currently is.")
                     << QStringLiteral("What may not work on Wayland: choosing a fixed screen coordinate or capturing the pointer position.")
                     << QStringLiteral("For full fixed-position support, use an X11 session or a desktop-specific trusted input method.");
    return caps;
}

bool UInputBackend::click(const ClickSettings& settings, QString* error)
{
    if (!initialize(error)) {
        return false;
    }

    if (settings.positionMode == PositionMode::FixedCoordinate) {
        if (error) {
            *error = QStringLiteral("Fixed-coordinate clicking is not available through the generic Wayland/uinput backend.");
        }
        return false;
    }

    const int repetitions = settings.clickType == ClickType::Double ? 2 : 1;

    if (settings.mouseButton == MouseButton::CustomKey) {
        const std::optional<Hotkey> key = parseHotkey(settings.customKey, error);
        if (!key) {
            return false;
        }

        const std::optional<int> code = qtKeyToUInputCode(key->key);
        if (!code) {
            if (error) {
                *error = QStringLiteral("Unsupported custom key for uinput.");
            }
            return false;
        }

        const std::vector<int> modifiers = modifierCodes(key->modifiers);
        for (int i = 0; i < repetitions; ++i) {
            for (int modifier : modifiers) {
                if (!emitEvent(EV_KEY, static_cast<unsigned short>(modifier), 1, error) || !sync(error)) {
                    return false;
                }
            }

            if (!emitEvent(EV_KEY, static_cast<unsigned short>(*code), 1, error) || !sync(error)
                || !emitEvent(EV_KEY, static_cast<unsigned short>(*code), 0, error) || !sync(error)) {
                return false;
            }

            for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
                if (!emitEvent(EV_KEY, static_cast<unsigned short>(*it), 0, error) || !sync(error)) {
                    return false;
                }
            }

            if (settings.clickType == ClickType::Double && i == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(35));
            }
        }

        return true;
    }

    const int button = uinputButton(settings.mouseButton);
    for (int i = 0; i < repetitions; ++i) {
        if (!emitEvent(EV_KEY, button, 1, error) || !sync(error)
            || !emitEvent(EV_KEY, button, 0, error) || !sync(error)) {
            return false;
        }

        if (settings.clickType == ClickType::Double && i == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(35));
        }
    }

    return true;
}

std::optional<QPoint> UInputBackend::currentPosition(QString* error)
{
    if (error) {
        *error = QStringLiteral("Wayland does not expose the global cursor position to normal applications.");
    }
    return std::nullopt;
}

bool UInputBackend::emitEvent(unsigned short type, unsigned short code, int value, QString* error)
{
    input_event event {};
    gettimeofday(&event.time, nullptr);
    event.type = type;
    event.code = code;
    event.value = value;

    const ssize_t written = write(fd_, &event, sizeof(event));
    if (written != static_cast<ssize_t>(sizeof(event))) {
        if (error) {
            *error = errnoMessage(QStringLiteral("Failed to write uinput event"));
        }
        return false;
    }

    return true;
}

bool UInputBackend::sync(QString* error)
{
    return emitEvent(EV_SYN, SYN_REPORT, 0, error);
}
