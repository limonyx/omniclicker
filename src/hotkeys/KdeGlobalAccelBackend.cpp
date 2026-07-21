#include "hotkeys/KdeGlobalAccelBackend.h"

#include "hotkeys/Hotkey.h"

#include <KGlobalAccel>

#include <QAction>

namespace {

constexpr const char* ActionUnique = "toggle-autoclicking";

QString actionIdForActivationId(const QString& activationId)
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
    if (id.startsWith(QLatin1Char('-'))) {
        id.remove(0, 1);
    }
    if (id.endsWith(QLatin1Char('-'))) {
        id.chop(1);
    }
    if (id.isEmpty()) {
        id = QStringLiteral("toggle");
    }
    return QStringLiteral("%1-%2").arg(QString::fromLatin1(ActionUnique), id);
}

} // namespace

KdeGlobalAccelBackend::KdeGlobalAccelBackend(QObject* parent)
    : QObject(parent)
{
}

KdeGlobalAccelBackend::~KdeGlobalAccelBackend()
{
    stop();
}

QString KdeGlobalAccelBackend::name() const
{
    return QStringLiteral("KDE KGlobalAccel");
}

bool KdeGlobalAccelBackend::start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error)
{
    stop();

    action_ = new QAction(this);
    action_->setObjectName(actionIdForActivationId(activationId));
    action_->setText(QStringLiteral("Toggle autoclicking %1").arg(activationId));

    callback_ = std::move(callback);
    connect(action_, &QAction::triggered, this, [this]() {
        if (callback_) {
            callback_();
        }
    });

    const QList<QKeySequence> shortcuts { hotkeyToKeySequence(hotkey) };
    KGlobalAccel* accel = KGlobalAccel::self();

    const bool defaultOk = accel->setDefaultShortcut(action_, shortcuts, KGlobalAccel::NoAutoloading);
    const bool shortcutOk = accel->setShortcut(action_, shortcuts, KGlobalAccel::NoAutoloading);

    if (!defaultOk || !shortcutOk || !accel->hasShortcut(action_)) {
        if (error) {
            *error = QStringLiteral("KDE Plasma did not accept the global shortcut. It may already be assigned in System Settings.");
        }
        stop();
        return false;
    }

    return true;
}

void KdeGlobalAccelBackend::stop()
{
    if (!action_) {
        return;
    }

    KGlobalAccel::self()->removeAllShortcuts(action_);
    action_->deleteLater();
    action_ = nullptr;
    callback_ = nullptr;
}

QString KdeGlobalAccelBackend::limitation() const
{
    return QStringLiteral("KDE Plasma global shortcuts are handled by the desktop. If a shortcut is already used, change it in the app or in KDE System Settings > Keyboard > Shortcuts.");
}
