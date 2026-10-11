#pragma once

#include "hotkeys/HotkeyBackend.h"

#include <QByteArray>

class NiriHotkeyBackend final : public HotkeyBackend {
public:
    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error) override;
    void stop() override;
    QString limitation() const override;

    static QString niriTrigger(const Hotkey& hotkey);
    static QString renderBinding(const Hotkey& hotkey, const QString& executable,
                                 const QString& activationId);
    static bool supportsVersion(const QString& version);
    static bool isNiriSession(const QString& desktop, const QByteArray& socketPath);

private:
    QString activationId_;
    bool active_ = false;
};
