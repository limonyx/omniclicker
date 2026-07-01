#pragma once

#include "backends/InputBackend.h"

class UInputBackend final : public InputBackend {
public:
    UInputBackend() = default;
    ~UInputBackend() override;

    QString name() const override;
    bool initialize(QString* error) override;
    BackendCapabilities capabilities() const override;
    bool click(const ClickSettings& settings, QString* error) override;
    std::optional<QPoint> currentPosition(QString* error) override;

private:
    bool emitEvent(unsigned short type, unsigned short code, int value, QString* error);
    bool sync(QString* error);

    int fd_ = -1;
};
