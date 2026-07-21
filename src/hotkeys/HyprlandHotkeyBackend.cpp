#include "hotkeys/HyprlandHotkeyBackend.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QProcess>
#include <QStringList>
#include <QUrl>
#include <QFile>

Q_LOGGING_CATEGORY(lcHyprlandHotkey, "omniclicker.hotkey.hyprland")

namespace {

/// Converts Qt modifiers to the Hyprland modifier string format.
/// Hyprland uses: SUPER, CTRL, ALT, SHIFT (space-separated for combos).
QString toHyprlandModifiers(Qt::KeyboardModifiers modifiers)
{
    QStringList parts;
    if (modifiers & Qt::MetaModifier) {
        parts << QStringLiteral("SUPER");
    }
    if (modifiers & Qt::ControlModifier) {
        parts << QStringLiteral("CTRL");
    }
    if (modifiers & Qt::AltModifier) {
        parts << QStringLiteral("ALT");
    }
    if (modifiers & Qt::ShiftModifier) {
        parts << QStringLiteral("SHIFT");
    }
    // Hyprland uses empty string for no modifiers
    return parts.isEmpty() ? QString() : parts.join(QStringLiteral(" "));
}

/// Converts a Qt key to the Hyprland key name.
/// Hyprland uses XKB key names — single letters are lowercase,
/// special keys like F1-F12, Return, etc. use their XKB names.
QString toHyprlandKey(int key)
{
    QString keyName = QKeySequence(key).toString(QKeySequence::PortableText);

    // Single character keys should be lowercase for Hyprland.
    if (keyName.length() == 1) {
        return keyName.toLower();
    }

    return keyName;
}

bool runHyprctl(const QStringList& args, QString* errorOut = nullptr)
{
    QProcess proc;
    if (QFile::exists(QStringLiteral("/.flatpak-info"))) {
        QStringList flatpakArgs;
        
        // Pass the HYPRLAND_INSTANCE_SIGNATURE to the host environment, 
        // as flatpak-spawn clears it by default.
        const QByteArray sig = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
        if (!sig.isEmpty()) {
            flatpakArgs << QStringLiteral("--env=HYPRLAND_INSTANCE_SIGNATURE=") + QString::fromUtf8(sig);
        }
        
        flatpakArgs << QStringLiteral("--host") << QStringLiteral("hyprctl") << args;
        proc.start(QStringLiteral("flatpak-spawn"), flatpakArgs);
    } else {
        proc.start(QStringLiteral("hyprctl"), args);
    }
    proc.waitForFinished(5000);

    if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        return true;
    }

    const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    qCWarning(lcHyprlandHotkey) << "hyprctl failed. Args:" << args << "Error:" << err;
    if (errorOut) {
        *errorOut = err;
    }
    return false;
}

} // namespace

HyprlandHotkeyBackend::~HyprlandHotkeyBackend()
{
    stop();
}

QString HyprlandHotkeyBackend::name() const
{
    return QStringLiteral("Hyprland IPC (hyprctl)");
}

bool HyprlandHotkeyBackend::start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error)
{
    stop();
    callback_ = std::move(callback);

    const QString modStr = toHyprlandModifiers(hotkey.modifiers);
    const QString keyStr = toHyprlandKey(hotkey.key);

    // Inside Flatpak, the host can't execute /app/bin/omniclicker directly.
    QString exe;
    if (QFile::exists(QStringLiteral("/.flatpak-info"))) {
        exe = QStringLiteral("flatpak run io.github.omniclicker");
    } else {
        exe = QCoreApplication::applicationFilePath();
    }

    // hyprctl keyword bind "MODS, KEY, exec, COMMAND"
    // Format: modifiers, key, dispatcher, params
    const QString encodedActivationId = QString::fromLatin1(QUrl::toPercentEncoding(activationId));
    const QString bindValue = QStringLiteral("%1, %2, exec, %3 --toggle-id \"%4\"")
                                  .arg(modStr, keyStr, exe, encodedActivationId);

    QString hyprError;
    if (!runHyprctl({ QStringLiteral("keyword"), QStringLiteral("bind"), bindValue }, &hyprError)) {
        if (error) {
            *error = QStringLiteral("Failed to register Hyprland keybinding via hyprctl: %1").arg(hyprError);
        }
        return false;
    }

    active_ = true;
    boundModifiers_ = modStr;
    boundKey_ = keyStr;
    qCInfo(lcHyprlandHotkey) << "Registered Hyprland keybinding:" << modStr << "+" << keyStr;
    return true;
}

void HyprlandHotkeyBackend::stop()
{
    if (!active_) {
        return;
    }

    active_ = false;

    if (!boundKey_.isEmpty()) {
        // hyprctl keyword unbind "MODS, KEY"
        const QString unbindValue = QStringLiteral("%1, %2").arg(boundModifiers_, boundKey_);
        runHyprctl({ QStringLiteral("keyword"), QStringLiteral("unbind"), unbindValue });
        qCInfo(lcHyprlandHotkey) << "Unregistered Hyprland keybinding:" << boundModifiers_ << "+" << boundKey_;
        boundModifiers_.clear();
        boundKey_.clear();
    }
}

QString HyprlandHotkeyBackend::limitation() const
{
    return QString();
}
