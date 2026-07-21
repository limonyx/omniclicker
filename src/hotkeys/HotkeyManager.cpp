#include "hotkeys/HotkeyManager.h"

#include "hotkeys/Hotkey.h"

#include <QMetaObject>
#include <QStringList>
#include <QLoggingCategory>
#include <QSet>

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
    return setHotkeyTexts(QStringList { hotkeyText });
}

bool GlobalHotkeyManager::setHotkeyTexts(const QStringList& hotkeyTexts)
{
    stop();

    QVector<Hotkey> hotkeys;
    QSet<QString> seen;
    for (const QString& hotkeyText : hotkeyTexts) {
        QString parseError;
        const std::optional<Hotkey> hotkey = parseHotkey(hotkeyText, &parseError);
        if (!hotkey) {
            qCWarning(lcHotkeyManager) << "Failed to parse hotkey:" << hotkeyText << "-" << parseError;
            emit errorOccurred(parseError);
            continue;
        }

        if (seen.contains(hotkey->normalized)) {
            continue;
        }

        seen.insert(hotkey->normalized);
        hotkeys.append(*hotkey);
    }

    if (hotkeys.isEmpty()) {
        emit errorOccurred(QStringLiteral("No valid global hotkeys to register."));
        return false;
    }

    bool anyStarted = false;
    QStringList errors;

    for (const Hotkey& hotkey : hotkeys) {
        qCInfo(lcHotkeyManager) << "Attempting to register global hotkey:" << hotkey.normalized;
        bool startedForHotkey = false;

        for (auto& candidate : createHotkeyBackendCandidates()) {
            QString startError;
            qCInfo(lcHotkeyManager) << "Trying backend:" << candidate->name();

            const QString normalized = hotkey.normalized;
            const bool started = candidate->start(hotkey, [this, normalized]() {
                QMetaObject::invokeMethod(this, [this, normalized]() {
                    emit hotkeyActivated(normalized);
                    emit activated();
                }, Qt::QueuedConnection);
            }, normalized, &startError);

            if (!started) {
                qCWarning(lcHotkeyManager) << "Backend" << candidate->name() << "failed to start:" << startError;
                errors << QStringLiteral("%1 (%2): %3").arg(hotkey.normalized, candidate->name(), startError);
                continue;
            }

            qCInfo(lcHotkeyManager) << "Successfully registered hotkey using backend:" << candidate->name();
            Registration registration;
            registration.hotkeyText = hotkey.normalized;
            registration.backend = std::move(candidate);
            emit statusChanged(QStringLiteral("%1 registered with %2. %3")
                                   .arg(hotkey.normalized, registration.backend->name(), registration.backend->limitation()));
            registrations_.push_back(std::move(registration));
            anyStarted = true;
            startedForHotkey = true;
            break;
        }

        if (!startedForHotkey) {
            qCWarning(lcHotkeyManager) << "No backend accepted hotkey:" << hotkey.normalized;
        }
    }

    if (!anyStarted) {
        qCCritical(lcHotkeyManager) << "All global hotkey backends failed to register.";
        emit errorOccurred(QStringLiteral("Global hotkey registration failed. Focused-window shortcut is still active.\n%1")
                               .arg(errors.join(QStringLiteral("\n"))));
    }
    return anyStarted;
}

void GlobalHotkeyManager::stop()
{
    for (Registration& registration : registrations_) {
        if (registration.backend) {
            qCInfo(lcHotkeyManager) << "Stopping active backend:" << registration.backend->name();
            registration.backend->stop();
            registration.backend.reset();
        }
    }
    registrations_.clear();
}
