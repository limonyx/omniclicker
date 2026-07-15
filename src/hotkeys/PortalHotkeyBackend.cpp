#include "hotkeys/PortalHotkeyBackend.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>

Q_LOGGING_CATEGORY(lcPortalHotkey, "omniclicker.hotkey.portal")

constexpr const char* PortalService = "org.freedesktop.portal.Desktop";
constexpr const char* PortalPath = "/org/freedesktop/portal/desktop";
constexpr const char* GlobalShortcutsInterface = "org.freedesktop.portal.GlobalShortcuts";
constexpr const char* RequestInterface = "org.freedesktop.portal.Request";
constexpr const char* SessionInterface = "org.freedesktop.portal.Session";

using PortalShortcut = QPair<QString, QVariantMap>;
Q_DECLARE_METATYPE(PortalShortcut)
Q_DECLARE_METATYPE(QList<PortalShortcut>)

inline QDBusArgument& operator<<(QDBusArgument& arg, const PortalShortcut& shortcut) {
    arg.beginStructure();
    arg << shortcut.first << shortcut.second;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument& operator>>(const QDBusArgument& arg, PortalShortcut& shortcut) {
    arg.beginStructure();
    arg >> shortcut.first >> shortcut.second;
    arg.endStructure();
    return arg;
}

QString keyNameForPortal(int key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
    }
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QChar(static_cast<char>('a' + key - Qt::Key_A));
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return QChar(static_cast<char>('0' + key - Qt::Key_0));
    }

    switch (key) {
    case Qt::Key_Space:
        return QStringLiteral("space");
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return QStringLiteral("Tab");
    case Qt::Key_Insert:
        return QStringLiteral("Insert");
    case Qt::Key_Delete:
        return QStringLiteral("Delete");
    case Qt::Key_Backspace:
        return QStringLiteral("BackSpace");
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QStringLiteral("Return");
    case Qt::Key_Escape:
        return QStringLiteral("Escape");
    case Qt::Key_Home:
        return QStringLiteral("Home");
    case Qt::Key_End:
        return QStringLiteral("End");
    case Qt::Key_PageUp:
        return QStringLiteral("Page_Up");
    case Qt::Key_PageDown:
        return QStringLiteral("Page_Down");
    case Qt::Key_Left:
        return QStringLiteral("Left");
    case Qt::Key_Right:
        return QStringLiteral("Right");
    case Qt::Key_Up:
        return QStringLiteral("Up");
    case Qt::Key_Down:
        return QStringLiteral("Down");
    case Qt::Key_Slash:
        return QStringLiteral("slash");
    case Qt::Key_Backslash:
        return QStringLiteral("backslash");
    case Qt::Key_Minus:
        return QStringLiteral("minus");
    case Qt::Key_Equal:
        return QStringLiteral("equal");
    case Qt::Key_Comma:
        return QStringLiteral("comma");
    case Qt::Key_Period:
        return QStringLiteral("period");
    case Qt::Key_Semicolon:
        return QStringLiteral("semicolon");
    case Qt::Key_Apostrophe:
        return QStringLiteral("apostrophe");
    case Qt::Key_BracketLeft:
        return QStringLiteral("bracketleft");
    case Qt::Key_BracketRight:
        return QStringLiteral("bracketright");
    case Qt::Key_QuoteLeft:
        return QStringLiteral("grave");
    default:
        return {};
    }
}

QString variantToPath(const QVariant& value)
{
    if (value.canConvert<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    return value.toString();
}

namespace {

bool portalHasRequiredMethods(QDBusConnection& bus)
{
    QDBusMessage introspect = QDBusMessage::createMethodCall(
        QString::fromLatin1(PortalService),
        QString::fromLatin1(PortalPath),
        QStringLiteral("org.freedesktop.DBus.Introspectable"),
        QStringLiteral("Introspect"));

    const QDBusMessage reply = bus.call(introspect, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        qCWarning(lcPortalHotkey) << "D-Bus introspection of portal object failed";
        return false;
    }

    const QString xml = reply.arguments().first().toString();
    const bool hasGlobalShortcuts = xml.contains(QStringLiteral("org.freedesktop.portal.GlobalShortcuts"));
    if (!hasGlobalShortcuts) {
        qCInfo(lcPortalHotkey) << "Introspection confirms GlobalShortcuts interface is not exposed";
        return false;
    }

    const bool hasCreateSession = xml.contains(QStringLiteral("CreateSession"));
    const bool hasBindShortcuts = xml.contains(QStringLiteral("BindShortcuts"));

    if (!hasCreateSession || !hasBindShortcuts) {
        qCWarning(lcPortalHotkey) << "GlobalShortcuts interface found but missing required methods"
                                  << "(CreateSession:" << hasCreateSession
                                  << "BindShortcuts:" << hasBindShortcuts << ")";
        return false;
    }

    qCInfo(lcPortalHotkey) << "Introspection confirms CreateSession and BindShortcuts methods available";
    return true;
}

} // namespace

PortalHotkeyBackend::PortalHotkeyBackend(QObject* parent)
    : QObject(parent)
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] PortalHotkeyBackend::PortalHotkeyBackend()";
}

PortalHotkeyBackend::~PortalHotkeyBackend()
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] PortalHotkeyBackend::~PortalHotkeyBackend()";
    stop();
}

QString PortalHotkeyBackend::name() const
{
    return QStringLiteral("xdg-desktop-portal GlobalShortcuts");
}

bool PortalHotkeyBackend::start(const Hotkey& hotkey, Callback callback, QString* error)
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] PortalHotkeyBackend::start()";
    stop();

    qCInfo(lcPortalHotkey) << "Attempting portal backend initialization";

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qCWarning(lcPortalHotkey) << "Cannot connect to session D-Bus";
        if (error) {
            *error = QStringLiteral("Cannot connect to the session D-Bus for xdg-desktop-portal global shortcuts.");
        }
        return false;
    }
    qCInfo(lcPortalHotkey) << "Session D-Bus connected (application connection)";

    static bool registryRegistered = false;
    if (!registryRegistered) {
        QDBusInterface registry(QString::fromLatin1(PortalService),
                                QString::fromLatin1(PortalPath),
                                QStringLiteral("org.freedesktop.host.portal.Registry"),
                                bus);
        if (registry.isValid()) {
            QString appId = QCoreApplication::applicationName();
#ifdef QT_GUI_LIB
            // If GUI module is available, prefer the desktop file name
            // though typically it's set in QGuiApplication. We'll fallback to a hardcoded one if needed.
#endif
            appId = QStringLiteral("io.github.omniclicker");
            qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Calling Register. Destination: org.freedesktop.portal.Desktop Interface: org.freedesktop.host.portal.Registry Method: Register args:" << appId;
            QDBusReply<void> regReply = registry.call(QStringLiteral("Register"), appId, QVariantMap());
            if (!regReply.isValid()) {
                qCWarning(lcPortalHotkey) << "[PORTAL DBUS LOG] Host portal registration failed. Error name:" << regReply.error().name() << "message:" << regReply.error().message();
            } else {
                qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Host portal registration succeeded.";
                registryRegistered = true;
            }
        }
    }

    QDBusConnectionInterface* busInterface = bus.interface();
    const QDBusReply<bool> portalRegistered = busInterface ? busInterface->isServiceRegistered(QString::fromLatin1(PortalService)) : QDBusReply<bool>();
    if (!portalRegistered.isValid() || !portalRegistered.value()) {
        qCWarning(lcPortalHotkey) << "xdg-desktop-portal service not registered on session bus";
        if (error) {
            *error = QStringLiteral("xdg-desktop-portal is not available on the session bus.");
        }
        return false;
    }
    qCInfo(lcPortalHotkey) << "xdg-desktop-portal service detected";

    QDBusInterface portal(QString::fromLatin1(PortalService),
                          QString::fromLatin1(PortalPath),
                          QString::fromLatin1(GlobalShortcutsInterface),
                          bus);
    if (!portal.isValid()) {
        qCWarning(lcPortalHotkey) << "GlobalShortcuts interface not available";
        if (error) {
            *error = QStringLiteral("The xdg-desktop-portal GlobalShortcuts interface is not available.");
        }
        return false;
    }
    qCInfo(lcPortalHotkey) << "GlobalShortcuts portal detected";

    if (!portalHasRequiredMethods(bus)) {
        qCWarning(lcPortalHotkey) << "GlobalShortcuts portal missing required methods";
        if (error) {
            *error = QStringLiteral("The GlobalShortcuts portal does not expose the required CreateSession/BindShortcuts methods.");
        }
        return false;
    }

    callback_ = std::move(callback);
    preferredTrigger_ = preferredTrigger(hotkey);
    active_ = true;

    qCInfo(lcPortalHotkey) << "Preferred trigger:" << preferredTrigger_;

    bus.connect(QString::fromLatin1(PortalService),
                QString::fromLatin1(PortalPath),
                QString::fromLatin1(GlobalShortcutsInterface),
                QStringLiteral("Activated"),
                this,
                SLOT(handleActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    qCInfo(lcPortalHotkey) << "Subscribed to Activated signal";

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), nextToken(QStringLiteral("create")));
    options.insert(QStringLiteral("session_handle_token"), nextToken(QStringLiteral("session")));

    QEventLoop loop;
    bool bindSuccess = false;
    QString bindError;
    auto finishConn = connect(this, &PortalHotkeyBackend::bindFinished, [&](bool success, const QString& err) {
        bindSuccess = success;
        bindError = err;
        loop.quit();
    });

    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Calling CreateSession. Destination: org.freedesktop.portal.Desktop Interface: org.freedesktop.portal.GlobalShortcuts Method: CreateSession args:" << options;
    QDBusReply<QDBusObjectPath> reply = portal.call(QStringLiteral("CreateSession"), options);
    if (!reply.isValid()) {
        qCWarning(lcPortalHotkey) << "[PORTAL DBUS LOG] CreateSession failed. Error name:" << reply.error().name() << "message:" << reply.error().message();
        if (error) {
            *error = QStringLiteral("Failed to create portal shortcut session: %1").arg(reply.error().message());
        }
        disconnect(finishConn);
        stop();
        return false;
    }

    createRequestPath_ = reply.value().path();
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] CreateSession returned path:" << createRequestPath_;

    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Connecting to Response signal on path:" << createRequestPath_;
    const bool connected = bus.connect(QString::fromLatin1(PortalService),
                                       createRequestPath_,
                                       QString::fromLatin1(RequestInterface),
                                       QStringLiteral("Response"),
                                       this,
                                       SLOT(handleCreateSessionResponse(uint,QVariantMap)));
    if (!connected) {
        qCWarning(lcPortalHotkey) << "Failed to connect to CreateSession Response signal";
        if (error) {
            *error = QStringLiteral("Failed to listen for the portal CreateSession response.");
        }
        disconnect(finishConn);
        stop();
        return false;
    }

    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();
    disconnect(finishConn);

    if (!bindSuccess) {
        if (error) {
            *error = bindError;
        }
        stop();
        return false;
    }

    qCInfo(lcPortalHotkey) << "Portal backend activated";
    return true;
}

void PortalHotkeyBackend::stop()
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] PortalHotkeyBackend::stop() called. active_ =" << active_;
    if (!active_) {
        return;
    }

    qCInfo(lcPortalHotkey) << "Stopping portal backend";

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        bus.disconnect(QString::fromLatin1(PortalService),
                       QString::fromLatin1(PortalPath),
                       QString::fromLatin1(GlobalShortcutsInterface),
                       QStringLiteral("Activated"),
                       this,
                       SLOT(handleActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
        disconnectRequest(createRequestPath_);
        disconnectRequest(bindRequestPath_);
        closeSession();
    }

    callback_ = nullptr;
    createRequestPath_.clear();
    bindRequestPath_.clear();
    sessionHandle_.clear();
    preferredTrigger_.clear();
    active_ = false;

    qCInfo(lcPortalHotkey) << "Portal backend stopped";
}

QString PortalHotkeyBackend::limitation() const
{
    return QStringLiteral("Wayland global shortcuts are mediated by xdg-desktop-portal. Your compositor may show a confirmation dialog and may choose a different shortcut.");
}

void PortalHotkeyBackend::handleCreateSessionResponse(uint response, const QVariantMap& results)
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] handleCreateSessionResponse() called. response code:" << response << "results:" << results;

    disconnectRequest(createRequestPath_);
    createRequestPath_.clear();

    if (!active_ || response != 0) {
        if (response != 0) {
            qCWarning(lcPortalHotkey) << "CreateSession rejected by portal, response code:" << response;
        }
        emit bindFinished(false, QStringLiteral("CreateSession rejected by portal (code %1)").arg(response));
        return;
    }

    sessionHandle_ = variantToPath(results.value(QStringLiteral("session_handle")));
    if (sessionHandle_.isEmpty()) {
        qCWarning(lcPortalHotkey) << "[PORTAL DBUS LOG] CreateSession response missing session handle";
        emit bindFinished(false, QStringLiteral("CreateSession response missing session handle"));
        return;
    }
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Session created. sessionHandle:" << sessionHandle_;

    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Subscribing to ShortcutsChanged and Closed on sessionHandle:" << sessionHandle_;
    QDBusConnection::sessionBus().connect(QString::fromLatin1(PortalService),
                                          sessionHandle_,
                                          QString::fromLatin1(SessionInterface),
                                          QStringLiteral("Closed"),
                                          this,
                                          SLOT(handleSessionClosed(QVariantMap)));
    // ShortcutsChanged might be on GlobalShortcuts interface but the object path is the session.
    // wait, the spec says ShortcutsChanged is on the GlobalShortcuts interface, but the object path is the session.
    // Let's connect to it.
    QDBusConnection::sessionBus().connect(QString::fromLatin1(PortalService),
                                          sessionHandle_,
                                          QString::fromLatin1(GlobalShortcutsInterface),
                                          QStringLiteral("ShortcutsChanged"),
                                          this,
                                          SLOT(handleShortcutsChanged(QDBusObjectPath,QVariant)));

    QDBusInterface portal(QString::fromLatin1(PortalService),
                          QString::fromLatin1(PortalPath),
                          QString::fromLatin1(GlobalShortcutsInterface),
                          QDBusConnection::sessionBus());

    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();

    QList<PortalShortcut> shortcuts;
    QVariantMap shortcutProperties;
    shortcutProperties.insert(QStringLiteral("description"), QStringLiteral("Start or stop autoclicking"));
    if (!preferredTrigger_.isEmpty()) {
        shortcutProperties.insert(QStringLiteral("preferred_trigger"), preferredTrigger_);
    }
    shortcuts.append(qMakePair(shortcutId_, shortcutProperties));

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), nextToken(QStringLiteral("bind")));

    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Calling BindShortcuts. Destination: org.freedesktop.portal.Desktop Interface: org.freedesktop.portal.GlobalShortcuts Method: BindShortcuts args: sessionHandle=" << sessionHandle_ << "shortcuts (size=" << shortcuts.size() << ") options=" << options;

    QDBusReply<QDBusObjectPath> reply = portal.call(QStringLiteral("BindShortcuts"),
                                                    QDBusObjectPath(sessionHandle_),
                                                    QVariant::fromValue(shortcuts),
                                                    QString(),
                                                    options);
    if (!reply.isValid()) {
        qCWarning(lcPortalHotkey) << "[PORTAL DBUS LOG] BindShortcuts call failed. Error name:" << reply.error().name() << "message:" << reply.error().message();
        emit bindFinished(false, QStringLiteral("BindShortcuts call failed: %1").arg(reply.error().message()));
        return;
    }

    bindRequestPath_ = reply.value().path();
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] BindShortcuts returned path:" << bindRequestPath_ << ". Connecting to Response signal...";

    QDBusConnection::sessionBus().connect(QString::fromLatin1(PortalService),
                                          bindRequestPath_,
                                          QString::fromLatin1(RequestInterface),
                                          QStringLiteral("Response"),
                                          this,
                                          SLOT(handleBindShortcutsResponse(uint,QVariantMap)));
}

void PortalHotkeyBackend::handleBindShortcutsResponse(uint response, const QVariantMap& results)
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] handleBindShortcutsResponse() called. response code:" << response << "results:" << results;

    disconnectRequest(bindRequestPath_);
    bindRequestPath_.clear();

    if (response != 0) {
        qCWarning(lcPortalHotkey) << "[PORTAL DBUS LOG] BindShortcuts rejected by portal, response code:" << response;
        emit bindFinished(false, QStringLiteral("BindShortcuts rejected by portal (code %1)").arg(response));
        return;
    }

    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] BindShortcuts succeeded";
    emit bindFinished(true, QString());

    const QVariant shortcutsResult = results.value(QStringLiteral("shortcuts"));
    if (shortcutsResult.isValid()) {
        const QDBusArgument argument = shortcutsResult.value<QDBusArgument>();
        if (argument.currentType() != QDBusArgument::ArrayType) {
            qCWarning(lcPortalHotkey) << "BindShortcuts response has unexpected argument type";
            return;
        }

        int count = 0;
        argument.beginArray();
        while (!argument.atEnd()) {
            QString id;
            QVariantMap props;
            argument.beginStructure();
            argument >> id >> props;
            argument.endStructure();
            const QString trigger = props.value(QStringLiteral("trigger_description")).toString();
            if (!trigger.isEmpty()) {
                qCInfo(lcPortalHotkey) << "  Shortcut" << id << "bound to" << trigger;
            } else {
                qCInfo(lcPortalHotkey) << "  Shortcut" << id << "bound (trigger assigned by compositor)";
            }
            count++;
        }
        argument.endArray();

        if (count == 0) {
            qCWarning(lcPortalHotkey) << "Portal returned empty shortcuts list";
            closeSession();
            return;
        }

        qCInfo(lcPortalHotkey) << "Portal bound" << count << "shortcut(s)";
    } else {
        qCInfo(lcPortalHotkey) << "BindShortcuts accepted (no shortcut details in response)";
    }
}

void PortalHotkeyBackend::handleActivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId, qulonglong timestamp, const QVariantMap& options)
{
    Q_UNUSED(timestamp)
    Q_UNUSED(options)

    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] handleActivated() called. sessionHandle:" << sessionHandle.path() << "shortcutId:" << shortcutId << "timestamp:" << timestamp << "options:" << options;

    if (!active_ || sessionHandle.path() != sessionHandle_ || shortcutId != shortcutId_) {
        qCDebug(lcPortalHotkey) << "[PORTAL DBUS LOG] Ignoring activation for different session or shortcut";
        return;
    }

    if (callback_) {
        qCInfo(lcPortalHotkey) << "Invoking toggle callback";
        callback_();
    }
}

QString PortalHotkeyBackend::preferredTrigger(const Hotkey& hotkey) const
{
    QStringList parts;
    if (hotkey.modifiers.testFlag(Qt::ControlModifier)) {
        parts << QStringLiteral("CTRL");
    }
    if (hotkey.modifiers.testFlag(Qt::AltModifier)) {
        parts << QStringLiteral("ALT");
    }
    if (hotkey.modifiers.testFlag(Qt::ShiftModifier)) {
        parts << QStringLiteral("SHIFT");
    }
    if (hotkey.modifiers.testFlag(Qt::MetaModifier)) {
        parts << QStringLiteral("LOGO");
    }

    const QString key = keyNameForPortal(hotkey.key);
    if (key.isEmpty()) {
        return {};
    }

    parts << key;
    return parts.join(QLatin1Char('+'));
}

QString PortalHotkeyBackend::nextToken(const QString& prefix) const
{
    const QString appName = QCoreApplication::applicationName().replace(QLatin1Char(' '), QLatin1Char('_')).toLower();
    return QStringLiteral("%1_%2_%3")
        .arg(prefix, appName)
        .arg(QRandomGenerator::global()->generate());
}

void PortalHotkeyBackend::disconnectRequest(const QString& path)
{
    if (path.isEmpty()) {
        return;
    }

    QDBusConnection::sessionBus().disconnect(QString::fromLatin1(PortalService),
                                             path,
                                             QString::fromLatin1(RequestInterface),
                                             QStringLiteral("Response"),
                                             this,
                                             nullptr);
}

void PortalHotkeyBackend::closeSession()
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] closeSession() called on sessionHandle:" << sessionHandle_;
    if (sessionHandle_.isEmpty()) {
        qCDebug(lcPortalHotkey) << "No session to close";
        return;
    }

    qCInfo(lcPortalHotkey) << "Closing GlobalShortcuts session" << sessionHandle_;

    QDBusInterface session(QString::fromLatin1(PortalService),
                           sessionHandle_,
                           QString::fromLatin1(SessionInterface),
                           QDBusConnection::sessionBus());

    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] Calling Close on session. Destination: org.freedesktop.portal.Desktop Interface: org.freedesktop.portal.Session Method: Close";
    session.asyncCall(QStringLiteral("Close"));

    qCInfo(lcPortalHotkey) << "Session closed successfully";
}

void PortalHotkeyBackend::handleSessionClosed(const QVariantMap& options)
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] handleSessionClosed() called. options:" << options;
}

void PortalHotkeyBackend::handleShortcutsChanged(const QDBusObjectPath& sessionHandle, const QVariant& shortcuts)
{
    qCInfo(lcPortalHotkey) << "[PORTAL DBUS LOG] handleShortcutsChanged() called. sessionHandle:" << sessionHandle.path() << "shortcuts:" << shortcuts;
}
