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

void loadClickingGroup(QSettings& settings, ClickSettings* result)
{
    result->intervalValue = settings.value(QStringLiteral("intervalValue"), result->intervalValue).toInt();
    result->intervalUnit = intervalUnitFromInt(settings.value(QStringLiteral("intervalUnit"), static_cast<int>(result->intervalUnit)).toInt());
    result->mouseButton = mouseButtonFromInt(settings.value(QStringLiteral("mouseButton"), static_cast<int>(result->mouseButton)).toInt());
    result->clickType = clickTypeFromInt(settings.value(QStringLiteral("clickType"), static_cast<int>(result->clickType)).toInt());
    result->positionMode = positionModeFromInt(settings.value(QStringLiteral("positionMode"), static_cast<int>(result->positionMode)).toInt());
    result->fixedPosition = QPoint(
        settings.value(QStringLiteral("fixedX"), result->fixedPosition.x()).toInt(),
        settings.value(QStringLiteral("fixedY"), result->fixedPosition.y()).toInt());
    result->customKey = settings.value(QStringLiteral("customKey"), result->customKey).toString();
    result->infinite = settings.value(QStringLiteral("infinite"), result->infinite).toBool();
    result->clickLimit = settings.value(QStringLiteral("clickLimit"), result->clickLimit).toULongLong();
    result->randomizeInterval = settings.value(QStringLiteral("randomizeInterval"), result->randomizeInterval).toBool();
    result->randomJitterPercent = settings.value(QStringLiteral("randomJitterPercent"), result->randomJitterPercent).toInt();
}

void saveClickingGroup(QSettings& settings, const ClickSettings& value)
{
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
}

} // namespace

ClickSettings AppConfig::load()
{
    const AppSettings appSettings = loadAppSettings();
    return appSettings.profiles.isEmpty() ? ClickSettings {} : appSettings.profiles.first().settings;
}

bool AppConfig::save(const ClickSettings& value, QString* error)
{
    ClickProfile profile;
    profile.settings = value;
    AppSettings appSettings;
    appSettings.profiles.append(profile);
    appSettings.startMinimized = value.startMinimized;
    appSettings.keepRunningInBackground = value.keepRunningInBackground;
    return saveAppSettings(appSettings, error);
}

AppSettings AppConfig::loadAppSettings()
{
    QSettings settings = openSettings();
    AppSettings result;

    settings.beginGroup(QStringLiteral("ui"));
    result.startMinimized = settings.value(QStringLiteral("startMinimized"), result.startMinimized).toBool();
    result.keepRunningInBackground = settings.value(QStringLiteral("keepRunningInBackground"), result.keepRunningInBackground).toBool();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("profiles"));
    const int count = settings.value(QStringLiteral("count"), 0).toInt();
    settings.endGroup();

    for (int i = 0; i < count; ++i) {
        ClickProfile profile;
        settings.beginGroup(QStringLiteral("profiles/%1").arg(i));
        profile.name = settings.value(QStringLiteral("name")).toString();
        profile.enabled = settings.value(QStringLiteral("enabled"), profile.enabled).toBool();
        loadClickingGroup(settings, &profile.settings);
        profile.settings.hotkey = settings.value(QStringLiteral("hotkey"), profile.settings.hotkey).toString();
        profile.settings.startMinimized = result.startMinimized;
        profile.settings.keepRunningInBackground = result.keepRunningInBackground;
        settings.endGroup();
        result.profiles.append(profile);
    }

    if (!result.profiles.isEmpty()) {
        return result;
    }

    ClickProfile migrated;
    settings.beginGroup(QStringLiteral("clicking"));
    loadClickingGroup(settings, &migrated.settings);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("hotkeys"));
    migrated.settings.hotkey = settings.value(QStringLiteral("toggle"), migrated.settings.hotkey).toString();
    settings.endGroup();

    migrated.settings.startMinimized = result.startMinimized;
    migrated.settings.keepRunningInBackground = result.keepRunningInBackground;
    result.profiles.append(migrated);
    return result;
}

bool AppConfig::saveAppSettings(const AppSettings& value, QString* error)
{
    QDir dir(configDirectory());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = QStringLiteral("Failed to create configuration directory: %1").arg(dir.absolutePath());
        }
        return false;
    }

    QSettings settings = openSettings();

    settings.remove(QStringLiteral("profiles"));

    settings.beginGroup(QStringLiteral("profiles"));
    settings.setValue(QStringLiteral("count"), value.profiles.size());
    settings.endGroup();

    for (int i = 0; i < value.profiles.size(); ++i) {
        const ClickProfile& profile = value.profiles.at(i);
        settings.beginGroup(QStringLiteral("profiles/%1").arg(i));
        settings.setValue(QStringLiteral("name"), profile.name);
        settings.setValue(QStringLiteral("enabled"), profile.enabled);
        saveClickingGroup(settings, profile.settings);
        settings.setValue(QStringLiteral("hotkey"), profile.settings.hotkey);
        settings.endGroup();
    }

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
