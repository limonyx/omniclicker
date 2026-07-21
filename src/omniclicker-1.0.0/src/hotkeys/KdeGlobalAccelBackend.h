#pragma once

#include "hotkeys/HotkeyBackend.h"

#include <QObject>

class QAction;

class KdeGlobalAccelBackend final : public QObject, public HotkeyBackend {
    Q_OBJECT

public:
    explicit KdeGlobalAccelBackend(QObject* parent = nullptr);
    ~KdeGlobalAccelBackend() override;

    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, QString* error) override;
    void stop() override;
    QString limitation() const override;

private:
    Callback callback_;
    QAction* action_ = nullptr;
};
