#pragma once

#include "Types.h"

#include <QString>

class AppConfig {
public:
    static ClickSettings load();
    static bool save(const ClickSettings& settings, QString* error = nullptr);

    static QString configPath();
};
