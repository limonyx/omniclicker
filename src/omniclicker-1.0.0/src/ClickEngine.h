#pragma once

#include "Types.h"

#include <QObject>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class ClickEngine final : public QObject {
    Q_OBJECT

public:
    explicit ClickEngine(QObject* parent = nullptr);
    ~ClickEngine() override;

    bool isRunning() const;
    quint64 clickCount() const;

public slots:
    void start(ClickSettings settings);
    void stop();
    void toggle(ClickSettings settings);

signals:
    void runningChanged(bool running);
    void clickCountChanged(quint64 count);
    void backendChanged(const QString& name, const QString& details);
    void errorOccurred(const QString& message);

private:
    void run(ClickSettings settings);
    bool waitUntilDeadline(std::chrono::steady_clock::time_point deadline,
                           std::chrono::milliseconds interval);
    int nextIntervalMs(const ClickSettings& settings) const;

    std::atomic_bool running_ = false;
    std::atomic<quint64> clickCount_ = 0;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
};
