#include "Config.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

QString configDirectory()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (path.isEmpty()) {
        path = QDir::homePath() + QStringLiteral("/.config/omniclicker");
    }

    return path;
}

QSettings openSettings()
{
    return QSettings(AppConfig::configPath(), QSettings::IniFormat);
}

} // namespace

ClickSettings AppConfig::load()
{
    QSettings settings = openSettings();
    ClickSettings result;

    settings.beginGroup(QStringLiteral("clicking"));
    result.intervalValue = settings.value(QStringLiteral("intervalValue"), result.intervalValue).toInt();
    result.intervalUnit = intervalUnitFromInt(settings.value(QStringLiteral("intervalUnit"), static_cast<int>(result.intervalUnit)).toInt());
    result.mouseButton = mouseButtonFromInt(settings.value(QStringLiteral("mouseButton"), static_cast<int>(result.mouseButton)).toInt());
    result.clickType = clickTypeFromInt(settings.value(QStringLiteral("clickType"), static_cast<int>(result.clickType)).toInt());
    result.positionMode = positionModeFromInt(settings.value(QStringLiteral("positionMode"), static_cast<int>(result.positionMode)).toInt());
    result.fixedPosition = QPoint(
        settings.value(QStringLiteral("fixedX"), result.fixedPosition.x()).toInt(),
        settings.value(QStringLiteral("fixedY"), result.fixedPosition.y()).toInt());
    result.customKey = settings.value(QStringLiteral("customKey"), result.customKey).toString();
    result.infinite = settings.value(QStringLiteral("infinite"), result.infinite).toBool();
    result.clickLimit = settings.value(QStringLiteral("clickLimit"), result.clickLimit).toULongLong();
    result.randomizeInterval = settings.value(QStringLiteral("randomizeInterval"), result.randomizeInterval).toBool();
    result.randomJitterPercent = settings.value(QStringLiteral("randomJitterPercent"), result.randomJitterPercent).toInt();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("hotkeys"));
    result.hotkey = settings.value(QStringLiteral("toggle"), result.hotkey).toString();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ui"));
    result.startMinimized = settings.value(QStringLiteral("startMinimized"), result.startMinimized).toBool();
    result.keepRunningInBackground = settings.value(QStringLiteral("keepRunningInBackground"), result.keepRunningInBackground).toBool();
    settings.endGroup();

    return result;
}

bool AppConfig::save(const ClickSettings& value, QString* error)
{
    QDir dir(configDirectory());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = QStringLiteral("Failed to create configuration directory: %1").arg(dir.absolutePath());
        }
        return false;
    }

    QSettings settings = openSettings();

    settings.beginGroup(QStringLiteral("clicking"));
    settings.setValue(QStringLiteral("intervalValue"), value.intervalValue);
    settings.setValue(QStringLiteral("intervalUnit"), static_cast<int>(value.intervalUnit));
    settings.setValue(QStringLiteral("mouseButton"), static_cast<int>(value.mouseButton));
    settings.setValue(QStringLiteral("clickType"), static_cast<int>(value.clickType));
    settings.setValue(QStringLiteral("positionMode"), static_cast<int>(value.positionMode));
    settings.setValue(QStringLiteral("fixedX"), value.fixedPosition.x());
    settings.setValue(QStringLiteral("fixedY"), value.fixedPosition.y());
    settings.setValue(QStringLiteral("customKey"), value.customKey);
    settings.setValue(QStringLiteral("infinite"), value.infinite);
    settings.setValue(QStringLiteral("clickLimit"), value.clickLimit);
    settings.setValue(QStringLiteral("randomizeInterval"), value.randomizeInterval);
    settings.setValue(QStringLiteral("randomJitterPercent"), value.randomJitterPercent);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("hotkeys"));
    settings.setValue(QStringLiteral("toggle"), value.hotkey);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("ui"));
    settings.setValue(QStringLiteral("startMinimized"), value.startMinimized);
    settings.setValue(QStringLiteral("keepRunningInBackground"), value.keepRunningInBackground);
    settings.endGroup();

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("Failed to write configuration: %1").arg(configPath());
        }
        return false;
    }

    return true;
}

QString AppConfig::configPath()
{
    return configDirectory() + QStringLiteral("/config.ini");
}
