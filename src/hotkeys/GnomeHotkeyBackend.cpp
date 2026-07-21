#include "hotkeys/GnomeHotkeyBackend.h"

#include <QCoreApplication>
#include <QFile>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QProcess>
#include <QUrl>

Q_LOGGING_CATEGORY(lcGnomeHotkey, "autoclicker.hotkey.gnome")

namespace {

const QString BasePath = QStringLiteral("org.gnome.settings-daemon.plugins.media-keys");
const QString CustomPathPrefix = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/omniclicker-");

bool isFlatpak()
{
    static const bool result = QFile::exists(QStringLiteral("/.flatpak-info"));
    return result;
}

bool runGsettings(const QStringList& args, QString* output = nullptr)
{
    QProcess proc;
    if (isFlatpak()) {
        QStringList flatpakArgs;
        flatpakArgs << QStringLiteral("--host") << QStringLiteral("gsettings") << args;
        proc.start(QStringLiteral("flatpak-spawn"), flatpakArgs);
    } else {
        proc.start(QStringLiteral("gsettings"), args);
    }
    proc.waitForFinished(5000);

    if (output) {
        *output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    }
    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

QString toGtkShortcut(const Hotkey& hotkey)
{
    QString seq = QKeySequence(hotkey.modifiers | hotkey.key).toString(QKeySequence::PortableText);
    seq.replace(QStringLiteral("Ctrl+"), QStringLiteral("<Primary>"));
    seq.replace(QStringLiteral("Shift+"), QStringLiteral("<Shift>"));
    seq.replace(QStringLiteral("Alt+"), QStringLiteral("<Alt>"));
    seq.replace(QStringLiteral("Meta+"), QStringLiteral("<Super>"));
    return seq;
}

QString shortcutPathForActivationId(const QString& activationId)
{
    QString id = activationId.toLower();
    for (QChar& ch : id) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('-');
        }
    }
    while (id.contains(QStringLiteral("--"))) {
        id.replace(QStringLiteral("--"), QStringLiteral("-"));
    }
    id = id.trimmed();
    if (id.startsWith(QLatin1Char('-'))) {
        id.remove(0, 1);
    }
    if (id.endsWith(QLatin1Char('-'))) {
        id.chop(1);
    }
    if (id.isEmpty()) {
        id = QStringLiteral("toggle");
    }
    return CustomPathPrefix + id + QLatin1Char('/');
}

} // namespace

GnomeHotkeyBackend::~GnomeHotkeyBackend()
{
    stop();
}

QString GnomeHotkeyBackend::name() const
{
    return QStringLiteral("GNOME Settings Custom Shortcut");
}

bool GnomeHotkeyBackend::start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error)
{
    stop();
    callback_ = std::move(callback);
    customPath_ = shortcutPathForActivationId(activationId);

    // Get current list of custom keybindings
    QString list;
    if (!runGsettings({ QStringLiteral("get"), BasePath, QStringLiteral("custom-keybindings") }, &list)) {
        if (error) {
            *error = QStringLiteral("Failed to read GNOME custom keybindings. Ensure gsettings is installed and accessible.");
        }
        return false;
    }

    if (list.startsWith(QStringLiteral("@as []"))) {
        list = QStringLiteral("[]");
    }

    // Add our custom path if not present
    if (!list.contains(QStringLiteral("'") + customPath_ + QStringLiteral("'"))) {
        if (list == QStringLiteral("[]")) {
            list = QStringLiteral("['%1']").arg(customPath_);
        } else if (list.endsWith(QStringLiteral("]"))) {
            list.chop(1);
            list += QStringLiteral(", '%1']").arg(customPath_);
        }

        if (!runGsettings({ QStringLiteral("set"), BasePath, QStringLiteral("custom-keybindings"), list })) {
            if (error) {
                *error = QStringLiteral("Failed to append to GNOME custom keybindings.");
            }
            return false;
        }
    }

    // Set properties for our shortcut
    const QString schemaPath = BasePath + QStringLiteral(".custom-keybinding:") + customPath_;

    runGsettings({ QStringLiteral("set"), schemaPath, QStringLiteral("name"), QStringLiteral("Omni Clicker Toggle %1").arg(activationId) });

    // Inside Flatpak, the toggle command must use 'flatpak run' since the host
    // can't directly execute a binary inside the sandbox.
    const QString encodedActivationId = QString::fromLatin1(QUrl::toPercentEncoding(activationId));
    QString exe;
    if (isFlatpak()) {
        exe = QStringLiteral("flatpak run io.github.omniclicker --toggle-id \"%1\"").arg(encodedActivationId);
    } else {
        exe = QCoreApplication::applicationFilePath() + QStringLiteral(" --toggle-id \"%1\"").arg(encodedActivationId);
    }
    runGsettings({ QStringLiteral("set"), schemaPath, QStringLiteral("command"), exe });

    const QString bindStr = toGtkShortcut(hotkey);
    runGsettings({ QStringLiteral("set"), schemaPath, QStringLiteral("binding"), bindStr });

    active_ = true;
    qCInfo(lcGnomeHotkey) << "Successfully registered GNOME custom shortcut:" << bindStr << "->" << exe;
    return true;
}

void GnomeHotkeyBackend::stop()
{
    if (!active_) {
        return;
    }
    
    active_ = false;
    removeFromGsettings();
}

void GnomeHotkeyBackend::removeFromGsettings()
{
    QString list;
    if (runGsettings({ QStringLiteral("get"), BasePath, QStringLiteral("custom-keybindings") }, &list)) {
        const QString toRemove = QStringLiteral("'") + customPath_ + QStringLiteral("'");
        if (list.contains(toRemove)) {
            list.replace(toRemove + QStringLiteral(", "), QStringLiteral(""));
            list.replace(QStringLiteral(", ") + toRemove, QStringLiteral(""));
            list.replace(toRemove, QStringLiteral(""));

            runGsettings({ QStringLiteral("set"), BasePath, QStringLiteral("custom-keybindings"), list });
        }
    }

    const QString schemaPath = BasePath + QStringLiteral(".custom-keybinding:") + customPath_;
    runGsettings({ QStringLiteral("reset-recursively"), schemaPath });
    customPath_.clear();
    
    qCInfo(lcGnomeHotkey) << "Removed GNOME custom shortcut.";
}

QString GnomeHotkeyBackend::limitation() const
{
    return QString();
}
