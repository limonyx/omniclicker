#include "hotkeys/HotkeyManager.h"

#include "hotkeys/Hotkey.h"

#include <QMetaObject>
#include <QStringList>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcHotkeyManager, "omniclicker.hotkey.manager")

GlobalHotkeyManager::GlobalHotkeyManager(QObject* parent)
    : QObject(parent)
{
}

GlobalHotkeyManager::~GlobalHotkeyManager()
{
    stop();
}

bool GlobalHotkeyManager::setHotkeyText(const QString& hotkeyText)
{
    QString parseError;
    const std::optional<Hotkey> hotkey = parseHotkey(hotkeyText, &parseError);
    if (!hotkey) {
        qCWarning(lcHotkeyManager) << "Failed to parse hotkey:" << hotkeyText << "-" << parseError;
        emit errorOccurred(parseError);
        return false;
    }

    qCInfo(lcHotkeyManager) << "Attempting to register global hotkey:" << hotkey->normalized;

    stop();

    QStringList errors;
    for (auto& candidate : createHotkeyBackendCandidates()) {
        QString startError;
        qCInfo(lcHotkeyManager) << "Trying backend:" << candidate->name();
        
        const bool started = candidate->start(*hotkey, [this]() {
            QMetaObject::invokeMethod(this, [this]() {
                emit activated();
            }, Qt::QueuedConnection);
        }, &startError);

        if (!started) {
            qCWarning(lcHotkeyManager) << "Backend" << candidate->name() << "failed to start:" << startError;
            errors << QStringLiteral("%1: %2").arg(candidate->name(), startError);
            continue;
        }

        qCInfo(lcHotkeyManager) << "Successfully registered hotkey using backend:" << candidate->name();
        backend_ = std::move(candidate);
        emit statusChanged(QStringLiteral("%1 registered with %2. %3")
                               .arg(hotkey->normalized, backend_->name(), backend_->limitation()));
        return true;
    }

    qCCritical(lcHotkeyManager) << "All global hotkey backends failed to register.";
    emit errorOccurred(QStringLiteral("Global hotkey registration failed. Focused-window shortcut is still active.\n%1")
                           .arg(errors.join(QStringLiteral("\n"))));
    backend_.reset();
    return false;
}

void GlobalHotkeyManager::stop()
{
    if (backend_) {
        qCInfo(lcHotkeyManager) << "Stopping active backend:" << backend_->name();
        backend_->stop();
        backend_.reset();
    }
}
