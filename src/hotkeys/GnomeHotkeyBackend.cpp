#include "hotkeys/GnomeHotkeyBackend.h"

#include <QCoreApplication>
#include <QFile>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QProcess>

Q_LOGGING_CATEGORY(lcGnomeHotkey, "autoclicker.hotkey.gnome")

namespace {

const QString BasePath = QStringLiteral("org.gnome.settings-daemon.plugins.media-keys");
const QString CustomPath = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/omniclicker/");

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

} // namespace

GnomeHotkeyBackend::~GnomeHotkeyBackend()
{
    stop();
}

QString GnomeHotkeyBackend::name() const
{
    return QStringLiteral("GNOME Settings Custom Shortcut");
}

bool GnomeHotkeyBackend::start(const Hotkey& hotkey, Callback callback, QString* error)
{
    stop();
    callback_ = std::move(callback);

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
    if (!list.contains(QStringLiteral("'") + CustomPath + QStringLiteral("'"))) {
        if (list == QStringLiteral("[]")) {
            list = QStringLiteral("['%1']").arg(CustomPath);
        } else if (list.endsWith(QStringLiteral("]"))) {
            list.chop(1);
            list += QStringLiteral(", '%1']").arg(CustomPath);
        }

        if (!runGsettings({ QStringLiteral("set"), BasePath, QStringLiteral("custom-keybindings"), list })) {
            if (error) {
                *error = QStringLiteral("Failed to append to GNOME custom keybindings.");
            }
            return false;
        }
    }

    // Set properties for our shortcut
    const QString schemaPath = BasePath + QStringLiteral(".custom-keybinding:") + CustomPath;

    runGsettings({ QStringLiteral("set"), schemaPath, QStringLiteral("name"), QStringLiteral("Omni Clicker Toggle") });

    // Inside Flatpak, the toggle command must use 'flatpak run' since the host
    // can't directly execute a binary inside the sandbox.
    QString exe;
    if (isFlatpak()) {
        exe = QStringLiteral("flatpak run io.github.omniclicker --toggle");
    } else {
        exe = QCoreApplication::applicationFilePath() + QStringLiteral(" --toggle");
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
        const QString toRemove = QStringLiteral("'") + CustomPath + QStringLiteral("'");
        if (list.contains(toRemove)) {
            list.replace(toRemove + QStringLiteral(", "), QStringLiteral(""));
            list.replace(QStringLiteral(", ") + toRemove, QStringLiteral(""));
            list.replace(toRemove, QStringLiteral(""));

            runGsettings({ QStringLiteral("set"), BasePath, QStringLiteral("custom-keybindings"), list });
        }
    }

    const QString schemaPath = BasePath + QStringLiteral(".custom-keybinding:") + CustomPath;
    runGsettings({ QStringLiteral("reset-recursively"), schemaPath });
    
    qCInfo(lcGnomeHotkey) << "Removed GNOME custom shortcut.";
}

QString GnomeHotkeyBackend::limitation() const
{
    return QString();
}
