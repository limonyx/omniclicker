#include "hotkeys/SwayHotkeyBackend.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QProcess>
#include <QStringList>
#include <QUrl>
#include <QFile>

Q_LOGGING_CATEGORY(lcSwayHotkey, "omniclicker.hotkey.sway")

namespace {

QString toSwayKeybind(const Hotkey& hotkey)
{
    QStringList parts;
    if (hotkey.modifiers & Qt::ControlModifier) {
        parts << QStringLiteral("Ctrl");
    }
    if (hotkey.modifiers & Qt::AltModifier) {
        parts << QStringLiteral("Mod1");
    }
    if (hotkey.modifiers & Qt::MetaModifier) {
        parts << QStringLiteral("Mod4");
    }
    if (hotkey.modifiers & Qt::ShiftModifier) {
        parts << QStringLiteral("Shift");
    }

    QString keyName = QKeySequence(hotkey.key).toString(QKeySequence::PortableText);

    // Sway expects lowercase key names when Shift is a separate modifier.
    if (keyName.length() == 1) {
        keyName = keyName.toLower();
    }

    parts << keyName;
    return parts.join(QStringLiteral("+"));
}

bool runSwaymsg(const QString& command, QString* errorOut = nullptr)
{
    QProcess proc;
    if (QFile::exists(QStringLiteral("/.flatpak-info"))) {
        proc.start(QStringLiteral("flatpak-spawn"), { QStringLiteral("--host"), QStringLiteral("swaymsg"), command });
    } else {
        proc.start(QStringLiteral("swaymsg"), { command });
    }
    proc.waitForFinished(5000);

    if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        return true;
    }

    const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    qCWarning(lcSwayHotkey) << "swaymsg failed for command:" << command << "error:" << err;
    if (errorOut) {
        *errorOut = err;
    }
    return false;
}

} // namespace

SwayHotkeyBackend::~SwayHotkeyBackend()
{
    stop();
}

QString SwayHotkeyBackend::name() const
{
    return QStringLiteral("Sway IPC (bindsym)");
}

bool SwayHotkeyBackend::start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error)
{
    stop();
    callback_ = std::move(callback);

    const QString bindStr = toSwayKeybind(hotkey);

    // Inside Flatpak, the host can't execute /app/bin/omniclicker directly.
    QString exe;
    if (QFile::exists(QStringLiteral("/.flatpak-info"))) {
        exe = QStringLiteral("flatpak run io.github.omniclicker");
    } else {
        exe = QCoreApplication::applicationFilePath();
    }

    // Use swaymsg to bind the key at runtime via IPC — no config files needed.
    const QString encodedActivationId = QString::fromLatin1(QUrl::toPercentEncoding(activationId));
    const QString cmd = QStringLiteral("bindsym %1 exec \"%2\" --toggle-id \"%3\"").arg(bindStr, exe, encodedActivationId);

    QString swayError;
    if (!runSwaymsg(cmd, &swayError)) {
        if (error) {
            *error = QStringLiteral("Failed to register Sway keybinding via swaymsg: %1").arg(swayError);
        }
        return false;
    }

    active_ = true;
    boundKey_ = bindStr;
    qCInfo(lcSwayHotkey) << "Registered Sway keybinding at runtime:" << bindStr;
    return true;
}

void SwayHotkeyBackend::stop()
{
    if (!active_) {
        return;
    }

    active_ = false;

    if (!boundKey_.isEmpty()) {
        const QString cmd = QStringLiteral("unbindsym %1").arg(boundKey_);
        runSwaymsg(cmd);
        qCInfo(lcSwayHotkey) << "Unregistered Sway keybinding:" << boundKey_;
        boundKey_.clear();
    }
}

QString SwayHotkeyBackend::limitation() const
{
    return QString();
}
