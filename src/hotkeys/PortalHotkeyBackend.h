#pragma once

#include "hotkeys/HotkeyBackend.h"

#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>

class PortalHotkeyBackend final : public QObject, public HotkeyBackend {
    Q_OBJECT

public:
    explicit PortalHotkeyBackend(QObject* parent = nullptr);
    ~PortalHotkeyBackend() override;

    QString name() const override;
    bool start(const Hotkey& hotkey, Callback callback, const QString& activationId, QString* error) override;
    void stop() override;
    QString limitation() const override;

signals:
    void bindFinished(bool success, const QString& error);

private slots:
    void handleCreateSessionResponse(uint response, const QVariantMap& results);
    void handleBindShortcutsResponse(uint response, const QVariantMap& results);
    void handleActivated(const QDBusObjectPath& sessionHandle, const QString& shortcutId, qulonglong timestamp, const QVariantMap& options);
    void handleSessionClosed(const QVariantMap& options);
    void handleShortcutsChanged(const QDBusObjectPath& sessionHandle, const QVariant& shortcuts);

private:
    QString preferredTrigger(const Hotkey& hotkey) const;
    QString nextToken(const QString& prefix) const;
    void disconnectRequest(const QString& path);
    void closeSession();

    Callback callback_;

    QString createRequestPath_;
    QString bindRequestPath_;
    QString sessionHandle_;
    QString shortcutId_ = QStringLiteral("toggle");
    QString preferredTrigger_;
    bool active_ = false;
};
