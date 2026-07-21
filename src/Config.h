#pragma once

#include "Types.h"

#include <QVector>
#include <QString>

struct ClickProfile {
    QString name;
    bool enabled = true;
    ClickSettings settings;
};

struct AppSettings {
    QVector<ClickProfile> profiles;
    bool startMinimized = false;
    bool keepRunningInBackground = false;
};

class AppConfig {
public:
    static ClickSettings load();
    static bool save(const ClickSettings& settings, QString* error = nullptr);
    static AppSettings loadAppSettings();
    static bool saveAppSettings(const AppSettings& appSettings, QString* error = nullptr);

    static QString configPath();
};
