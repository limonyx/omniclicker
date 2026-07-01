#include "hotkeys/KdeGlobalAccelBackend.h"

#include "hotkeys/Hotkey.h"

#include <KGlobalAccel>

#include <QAction>

namespace {

constexpr const char* ActionUnique = "toggle-autoclicking";

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

bool KdeGlobalAccelBackend::start(const Hotkey& hotkey, Callback callback, QString* error)
{
    stop();

    action_ = new QAction(this);
    action_->setObjectName(QString::fromLatin1(ActionUnique));
    action_->setText(QStringLiteral("Toggle autoclicking"));

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
