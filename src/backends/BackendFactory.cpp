#include "backends/InputBackend.h"

#include "backends/UInputBackend.h"
#include "backends/X11Backend.h"

#include <QByteArray>

namespace {

QString envLower(const char* name)
{
    return QString::fromLocal8Bit(qgetenv(name)).trimmed().toLower();
}

bool hasEnv(const char* name)
{
    return !qgetenv(name).isEmpty();
}

} // namespace

QString detectedSessionType()
{
    const QString forced = envLower("OMNICLICKER_BACKEND");
    if (forced == QStringLiteral("x11") || forced == QStringLiteral("uinput") || forced == QStringLiteral("wayland")) {
        return forced;
    }

    const QString session = envLower("XDG_SESSION_TYPE");
    if (!session.isEmpty()) {
        return session;
    }

    if (hasEnv("WAYLAND_DISPLAY")) {
        return QStringLiteral("wayland");
    }

    if (hasEnv("DISPLAY")) {
        return QStringLiteral("x11");
    }

    return QStringLiteral("unknown");
}

std::unique_ptr<InputBackend> createInputBackend(QString* error)
{
    const QString forced = envLower("OMNICLICKER_BACKEND");
    const QString session = detectedSessionType();

    if (forced == QStringLiteral("x11")) {
        return std::make_unique<X11Backend>();
    }

    if (forced == QStringLiteral("uinput") || forced == QStringLiteral("wayland")) {
        return std::make_unique<UInputBackend>();
    }

    if (session == QStringLiteral("wayland")) {
        return std::make_unique<UInputBackend>();
    }

    if (session == QStringLiteral("x11") || hasEnv("DISPLAY")) {
        return std::make_unique<X11Backend>();
    }

    if (error) {
        *error = QStringLiteral("Unable to detect a supported display server. Set OMNICLICKER_BACKEND=x11 or OMNICLICKER_BACKEND=uinput to force a backend.");
    }
    return nullptr;
}
