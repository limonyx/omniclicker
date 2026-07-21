#include "ClickEngine.h"

#include "backends/InputBackend.h"

#include <QRandomGenerator>

#include <algorithm>
#include <limits>
#include <chrono>
#include <thread>

namespace {

// Keep this small: the engine sleeps for almost the whole interval and only
// spins at the end to reduce scheduler wakeup jitter. Raising this improves
// sub-millisecond deadline precision but costs more CPU at very low intervals.
constexpr std::chrono::microseconds kFinalSpinWindow { 100 };

} // namespace

ClickEngine::ClickEngine(QObject* parent)
    : QObject(parent)
{
}

ClickEngine::~ClickEngine()
{
    stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

bool ClickEngine::isRunning() const
{
    return running_.load();
}

quint64 ClickEngine::clickCount() const
{
    return clickCount_.load();
}

void ClickEngine::start(ClickSettings settings)
{
    if (running_.exchange(true)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.join();
    }

    clickCount_ = 0;
    emit clickCountChanged(0);
    emit runningChanged(true);
    worker_ = std::thread(&ClickEngine::run, this, settings);
}

void ClickEngine::stop()
{
    if (!running_.exchange(false)) {
        return;
    }

    wake_.notify_all();
    if (worker_.joinable() && worker_.get_id() != std::this_thread::get_id()) {
        worker_.join();
    }
    emit runningChanged(false);
}

void ClickEngine::toggle(ClickSettings settings)
{
    if (isRunning()) {
        stop();
    } else {
        start(settings);
    }
}

void ClickEngine::run(ClickSettings settings)
{
    QString backendError;
    std::unique_ptr<InputBackend> backend = createInputBackend(&backendError);
    if (!backend) {
        emit errorOccurred(backendError);
        running_ = false;
        emit runningChanged(false);
        return;
    }

    if (!backend->initialize(&backendError)) {
        emit errorOccurred(backendError);
        running_ = false;
        emit runningChanged(false);
        return;
    }

    const BackendCapabilities caps = backend->capabilities();
    emit backendChanged(backend->name(), caps.limitations.join(QStringLiteral("\n")));

    auto nextDeadline = std::chrono::steady_clock::now();

    while (running_.load()) {
        if (!settings.infinite && clickCount_.load() >= settings.clickLimit) {
            break;
        }

        QString clickError;
        if (!backend->click(settings, &clickError)) {
            emit errorOccurred(clickError);
            break;
        }

        const quint64 count = clickCount_.fetch_add(1) + 1;
        emit clickCountChanged(count);

        const std::chrono::milliseconds interval(nextIntervalMs(settings));
        // Use absolute deadlines so backend latency and small scheduler
        // oversleeps do not accumulate into long-term drift. If the backend
        // takes longer than the requested period, skip missed periods instead
        // of firing a burst of catch-up clicks.
        nextDeadline += interval;
        const auto now = std::chrono::steady_clock::now();
        if (nextDeadline <= now) {
            const auto missedPeriods = ((now - nextDeadline) / interval) + 1;
            nextDeadline += missedPeriods * interval;
        }

        if (!waitUntilDeadline(nextDeadline, interval)) {
            break;
        }
    }

    running_ = false;
    emit runningChanged(false);
}

bool ClickEngine::waitUntilDeadline(std::chrono::steady_clock::time_point deadline,
                                    std::chrono::milliseconds interval)
{
    const auto spinWindow = std::min<std::chrono::steady_clock::duration>(
        kFinalSpinWindow,
        std::max<std::chrono::steady_clock::duration>(std::chrono::microseconds(0), interval / 10));
    const auto sleepDeadline = deadline - spinWindow;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (wake_.wait_until(lock, sleepDeadline, [this]() {
                return !running_.load();
            })) {
            return false;
        }
    }

    // No permanent busy-wait: this path is only the final, bounded precision
    // window before the absolute deadline. stop() still exits promptly because
    // running_ is checked on every spin iteration.
    while (running_.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    return running_.load();
}

int ClickEngine::nextIntervalMs(const ClickSettings& settings) const
{
    const int base = std::max(1, settings.intervalMilliseconds());
    if (!settings.randomizeInterval || settings.randomJitterPercent <= 0) {
        return base;
    }

    const qint64 jitter = std::max<qint64>(1,
        (static_cast<qint64>(base) * std::clamp(settings.randomJitterPercent, 0, 95)) / 100);
    const int low = static_cast<int>(std::max<qint64>(1, static_cast<qint64>(base) - jitter));
    const int high = static_cast<int>(std::min<qint64>(std::numeric_limits<int>::max(),
                                                       static_cast<qint64>(base) + jitter));
    return high == std::numeric_limits<int>::max()
        ? QRandomGenerator::global()->bounded(low, high)
        : QRandomGenerator::global()->bounded(low, high + 1);
}
