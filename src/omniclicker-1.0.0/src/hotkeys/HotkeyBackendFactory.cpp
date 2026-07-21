#include "hotkeys/HotkeyBackend.h"

#include "backends/InputBackend.h"
#include "hotkeys/EvdevHotkeyBackend.h"
#include "hotkeys/GnomeHotkeyBackend.h"
#include "hotkeys/HyprlandHotkeyBackend.h"
#include "hotkeys/PortalHotkeyBackend.h"
#include "hotkeys/SwayHotkeyBackend.h"
#include "hotkeys/X11HotkeyBackend.h"

#ifdef HAVE_KGLOBALACCEL
#include "hotkeys/KdeGlobalAccelBackend.h"
#endif

#include <QByteArray>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcBackendFactory, "omniclicker.hotkey.factory")

namespace {

QString forcedBackend()
{
    return QString::fromLocal8Bit(qgetenv("OMNICLICKER_HOTKEY_BACKEND")).trimmed().toLower();
}

QString currentDesktop()
{
    return QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")).trimmed().toUpper();
}

bool isHyprland(const QString& desktop)
{
    // Check XDG_CURRENT_DESKTOP first, then fall back to HYPRLAND_INSTANCE_SIGNATURE
    // which is always set by the Hyprland compositor.
    return desktop.contains(QStringLiteral("HYPRLAND"))
           || !qgetenv("HYPRLAND_INSTANCE_SIGNATURE").isEmpty();
}

} // namespace

std::unique_ptr<HotkeyBackend> createHotkeyBackend()
{
    auto candidates = createHotkeyBackendCandidates();
    if (candidates.empty()) {
        return nullptr;
    }
    return std::move(candidates.front());
}

std::vector<std::unique_ptr<HotkeyBackend>> createHotkeyBackendCandidates()
{
    std::vector<std::unique_ptr<HotkeyBackend>> candidates;
    const QString forced = forcedBackend();
    
    if (!forced.isEmpty()) {
        qCInfo(lcBackendFactory) << "Forced hotkey backend via environment:" << forced;
    }

    if (forced == QStringLiteral("x11")) {
        candidates.push_back(std::make_unique<X11HotkeyBackend>());
        return candidates;
    }
    if (forced == QStringLiteral("evdev")) {
        candidates.push_back(std::make_unique<EvdevHotkeyBackend>());
        return candidates;
    }
    if (forced == QStringLiteral("portal")) {
        candidates.push_back(std::make_unique<PortalHotkeyBackend>());
        return candidates;
    }
    if (forced == QStringLiteral("hyprland")) {
        candidates.push_back(std::make_unique<HyprlandHotkeyBackend>());
        return candidates;
    }
    if (forced == QStringLiteral("sway")) {
        candidates.push_back(std::make_unique<SwayHotkeyBackend>());
        return candidates;
    }
    if (forced == QStringLiteral("kde") || forced == QStringLiteral("kglobalaccel")) {
#ifdef HAVE_KGLOBALACCEL
        candidates.push_back(std::make_unique<KdeGlobalAccelBackend>());
#endif
        return candidates;
    }

    const QString session = detectedSessionType();
    const QString desktop = currentDesktop();
    qCInfo(lcBackendFactory) << "Detected session type:" << session;
    qCInfo(lcBackendFactory) << "Detected desktop environment:" << desktop;

    if (session == QStringLiteral("wayland")) {
        if (desktop.contains(QStringLiteral("KDE"))) {
            qCInfo(lcBackendFactory) << "KDE Plasma Wayland detected. Preferring native KGlobalAccel over Portal.";
#ifdef HAVE_KGLOBALACCEL
            candidates.push_back(std::make_unique<KdeGlobalAccelBackend>());
#endif
            candidates.push_back(std::make_unique<PortalHotkeyBackend>());
            candidates.push_back(std::make_unique<X11HotkeyBackend>());
            candidates.push_back(std::make_unique<EvdevHotkeyBackend>());
        } else if (isHyprland(desktop)) {
            qCInfo(lcBackendFactory) << "Hyprland detected. Preferring Hyprland IPC.";
            candidates.push_back(std::make_unique<HyprlandHotkeyBackend>());
            candidates.push_back(std::make_unique<PortalHotkeyBackend>());
            candidates.push_back(std::make_unique<EvdevHotkeyBackend>());
        } else if (desktop.contains(QStringLiteral("SWAY"))) {
            qCInfo(lcBackendFactory) << "Sway detected. Preferring Sway native configuration.";
            candidates.push_back(std::make_unique<SwayHotkeyBackend>());
            candidates.push_back(std::make_unique<PortalHotkeyBackend>());
            candidates.push_back(std::make_unique<EvdevHotkeyBackend>());
        } else if (desktop.contains(QStringLiteral("GNOME"))) {
            qCInfo(lcBackendFactory) << "GNOME Wayland detected. Preferring GNOME native configuration.";
            candidates.push_back(std::make_unique<GnomeHotkeyBackend>());
            candidates.push_back(std::make_unique<PortalHotkeyBackend>());
            candidates.push_back(std::make_unique<EvdevHotkeyBackend>());
        } else {
            qCInfo(lcBackendFactory) << "Wayland session detected. Preferring XDG Desktop Portal.";
            candidates.push_back(std::make_unique<PortalHotkeyBackend>());
            candidates.push_back(std::make_unique<EvdevHotkeyBackend>());
        }
        return candidates;
    }

    qCInfo(lcBackendFactory) << "Defaulting to X11 hotkey backend.";
    candidates.push_back(std::make_unique<X11HotkeyBackend>());
    return candidates;
}
