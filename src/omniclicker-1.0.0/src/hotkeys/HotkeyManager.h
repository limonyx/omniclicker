#pragma once

#include "hotkeys/HotkeyBackend.h"

#include <QObject>

class GlobalHotkeyManager final : public QObject {
    Q_OBJECT

public:
    explicit GlobalHotkeyManager(QObject* parent = nullptr);
    ~GlobalHotkeyManager() override;

    bool setHotkeyText(const QString& hotkeyText);
    void stop();

signals:
    void activated();
    void errorOccurred(const QString& message);
    void statusChanged(const QString& message);

private:
    std::unique_ptr<HotkeyBackend> backend_;
};
