#pragma once

#include "hotkeys/HotkeyBackend.h"

#include <QObject>
#include <QStringList>

#include <vector>

class GlobalHotkeyManager final : public QObject {
    Q_OBJECT

public:
    explicit GlobalHotkeyManager(QObject* parent = nullptr);
    ~GlobalHotkeyManager() override;

    bool setHotkeyText(const QString& hotkeyText);
    bool setHotkeyTexts(const QStringList& hotkeyTexts);
    void stop();

signals:
    void activated();
    void hotkeyActivated(const QString& hotkeyText);
    void errorOccurred(const QString& message);
    void statusChanged(const QString& message);

private:
    struct Registration {
        QString hotkeyText;
        std::unique_ptr<HotkeyBackend> backend;
    };

    std::vector<Registration> registrations_;
};
