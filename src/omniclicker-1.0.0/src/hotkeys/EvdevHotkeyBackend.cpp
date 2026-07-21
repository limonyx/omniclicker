#include "hotkeys/EvdevHotkeyBackend.h"

#include <QDir>

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>

namespace {

std::optional<unsigned short> qtKeyToEvdev(int key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return static_cast<unsigned short>(KEY_F1 + key - Qt::Key_F1);
    }
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
        return static_cast<unsigned short>(KEY_1 + key - Qt::Key_1);
    }
    if (key == Qt::Key_0) {
        return KEY_0;
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

bool hasAny(const std::set<unsigned short>& pressed, std::initializer_list<unsigned short> keys)
{
    for (const unsigned short key : keys) {
        if (pressed.find(key) != pressed.end()) {
            return true;
        }
    }
    return false;
}

} // namespace

EvdevHotkeyBackend::~EvdevHotkeyBackend()
{
    stop();
}

QString EvdevHotkeyBackend::name() const
{
    return QStringLiteral("evdev keyboard monitor");
}

bool EvdevHotkeyBackend::start(const Hotkey& hotkey, Callback callback, QString* error)
{
    stop();

    const std::optional<unsigned short> evdevCode = qtKeyToEvdev(hotkey.key);
    if (!evdevCode) {
        if (error) {
            *error = QStringLiteral("Unsupported hotkey key for evdev backend.");
        }
        return false;
    }

    targetCode_ = *evdevCode;
    targetModifiers_ = hotkey.modifiers;
    callback_ = std::move(callback);

    const QDir inputDir(QStringLiteral("/dev/input"));
    const QStringList devices = inputDir.entryList(QStringList() << QStringLiteral("event*"), QDir::System | QDir::Readable | QDir::NoDotAndDotDot);

    for (const QString& device : devices) {
        const QString path = inputDir.absoluteFilePath(device);
        const int fd = open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd >= 0) {
            fds_.push_back(fd);
        }
    }

    if (fds_.empty()) {
        if (error) {
            *error = QStringLiteral("Cannot read /dev/input/event*. Wayland global hotkeys need evdev read permission, usually via the input group.");
        }
        return false;
    }

    running_ = true;
    thread_ = std::thread(&EvdevHotkeyBackend::eventLoop, this);
    return true;
}

void EvdevHotkeyBackend::stop()
{
    if (!running_.exchange(false)) {
        return;
    }

    if (thread_.joinable()) {
        thread_.join();
    }

    for (const int fd : fds_) {
        close(fd);
    }
    fds_.clear();
    pressed_.clear();
}

QString EvdevHotkeyBackend::limitation() const
{
    return QStringLiteral("Wayland has no universal global-hotkey API. The evdev fallback requires /dev/input/event* read permission and should be granted only to trusted users.");
}

void EvdevHotkeyBackend::eventLoop()
{
    auto lastActivation = std::chrono::steady_clock::now() - std::chrono::seconds(1);

    while (running_.load()) {
        std::vector<pollfd> pollFds;
        pollFds.reserve(fds_.size());
        for (const int fd : fds_) {
            pollFds.push_back({ fd, POLLIN, 0 });
        }

        const int ready = poll(pollFds.data(), pollFds.size(), 100);
        if (ready <= 0) {
            continue;
        }

        for (const pollfd& polled : pollFds) {
            if ((polled.revents & POLLIN) == 0) {
                continue;
            }

            input_event event {};
            while (read(polled.fd, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
                if (event.type != EV_KEY) {
                    continue;
                }

                if (event.value == 0) {
                    pressed_.erase(event.code);
                    continue;
                }

                if (event.value == 1 || event.value == 2) {
                    pressed_.insert(event.code);
                }

                if (event.value == 1 && event.code == targetCode_ && modifiersMatch()) {
                    const auto now = std::chrono::steady_clock::now();
                    if (now - lastActivation > std::chrono::milliseconds(250)) {
                        lastActivation = now;
                        if (callback_) {
                            callback_();
                        }
                    }
                }
            }
        }
    }
}

bool EvdevHotkeyBackend::modifiersMatch() const
{
    const bool ctrl = hasAny(pressed_, { KEY_LEFTCTRL, KEY_RIGHTCTRL });
    const bool alt = hasAny(pressed_, { KEY_LEFTALT, KEY_RIGHTALT });
    const bool shift = hasAny(pressed_, { KEY_LEFTSHIFT, KEY_RIGHTSHIFT });
    const bool meta = hasAny(pressed_, { KEY_LEFTMETA, KEY_RIGHTMETA });

    return ctrl == targetModifiers_.testFlag(Qt::ControlModifier)
        && alt == targetModifiers_.testFlag(Qt::AltModifier)
        && shift == targetModifiers_.testFlag(Qt::ShiftModifier)
        && meta == targetModifiers_.testFlag(Qt::MetaModifier);
}
