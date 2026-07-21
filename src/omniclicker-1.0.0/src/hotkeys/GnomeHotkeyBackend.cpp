#include "hotkeys/GnomeHotkeyBackend.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QProcess>

Q_LOGGING_CATEGORY(lcGnomeHotkey, "autoclicker.hotkey.gnome")

namespace {

const QString BasePath = QStringLiteral("org.gnome.settings-daemon.plugins.media-keys");
const QString CustomPath = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/omniclicker/");

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
    QProcess getProc;
    getProc.start(QStringLiteral("gsettings"), { QStringLiteral("get"), BasePath, QStringLiteral("custom-keybindings") });
    getProc.waitForFinished();
    if (getProc.exitCode() != 0) {
        if (error) {
            *error = QStringLiteral("Failed to read GNOME custom keybindings. Ensure gsettings is installed and accessible.");
        }
        return false;
    }

    QString list = QString::fromUtf8(getProc.readAllStandardOutput()).trimmed();
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

        QProcess setProc;
        setProc.start(QStringLiteral("gsettings"), { QStringLiteral("set"), BasePath, QStringLiteral("custom-keybindings"), list });
        setProc.waitForFinished();
        if (setProc.exitCode() != 0) {
            if (error) {
                *error = QStringLiteral("Failed to append to GNOME custom keybindings.");
            }
            return false;
        }
    }

    // Set properties for our shortcut
    const QString schemaPath = BasePath + QStringLiteral(".custom-keybinding:") + CustomPath;

    QProcess proc;
    proc.start(QStringLiteral("gsettings"), { QStringLiteral("set"), schemaPath, QStringLiteral("name"), QStringLiteral("Omni Clicker Toggle") });
    proc.waitForFinished();

    const QString exe = QCoreApplication::applicationFilePath() + QStringLiteral(" --toggle");
    proc.start(QStringLiteral("gsettings"), { QStringLiteral("set"), schemaPath, QStringLiteral("command"), exe });
    proc.waitForFinished();

    const QString bindStr = toGtkShortcut(hotkey);
    proc.start(QStringLiteral("gsettings"), { QStringLiteral("set"), schemaPath, QStringLiteral("binding"), bindStr });
    proc.waitForFinished();

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
    QProcess getProc;
    getProc.start(QStringLiteral("gsettings"), { QStringLiteral("get"), BasePath, QStringLiteral("custom-keybindings") });
    getProc.waitForFinished();
    if (getProc.exitCode() == 0) {
        QString list = QString::fromUtf8(getProc.readAllStandardOutput()).trimmed();
        const QString toRemove = QStringLiteral("'") + CustomPath + QStringLiteral("'");
        if (list.contains(toRemove)) {
            list.replace(toRemove + QStringLiteral(", "), QStringLiteral(""));
            list.replace(QStringLiteral(", ") + toRemove, QStringLiteral(""));
            list.replace(toRemove, QStringLiteral(""));

            QProcess setProc;
            setProc.start(QStringLiteral("gsettings"), { QStringLiteral("set"), BasePath, QStringLiteral("custom-keybindings"), list });
            setProc.waitForFinished();
        }
    }

    const QString schemaPath = BasePath + QStringLiteral(".custom-keybinding:") + CustomPath;
    QProcess resetProc;
    resetProc.start(QStringLiteral("gsettings"), { QStringLiteral("reset-recursively"), schemaPath });
    resetProc.waitForFinished();
    
    qCInfo(lcGnomeHotkey) << "Removed GNOME custom shortcut.";
}

QString GnomeHotkeyBackend::limitation() const
{
    return QString();
}
