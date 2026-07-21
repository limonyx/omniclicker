#pragma once

#include "Types.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

struct BackendCapabilities {
    bool supportsCurrentPositionClicking = true;
    bool supportsFixedPositionClicking = false;
    bool supportsCursorCapture = false;
    QStringList limitations;
};

class InputBackend {
public:
    virtual ~InputBackend() = default;

    virtual QString name() const = 0;
    virtual bool initialize(QString* error) = 0;
    virtual BackendCapabilities capabilities() const = 0;
    virtual bool click(const ClickSettings& settings, QString* error) = 0;
    virtual std::optional<QPoint> currentPosition(QString* error) = 0;
};

QString detectedSessionType();
std::unique_ptr<InputBackend> createInputBackend(QString* error = nullptr);
