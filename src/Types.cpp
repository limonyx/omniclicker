#include "Types.h"

#include <algorithm>
#include <limits>

int ClickSettings::intervalMilliseconds() const
{
    const int safeValue = std::max(1, intervalValue);

    qint64 multiplier = 1;
    switch (intervalUnit) {
    case IntervalUnit::Milliseconds:
        break;
    case IntervalUnit::Seconds:
        multiplier = 1000;
        break;
    case IntervalUnit::Minutes:
        multiplier = 60 * 1000;
        break;
    case IntervalUnit::Hours:
        multiplier = 60 * 60 * 1000;
        break;
    }

    return static_cast<int>(std::min<qint64>(static_cast<qint64>(safeValue) * multiplier,
                                              std::numeric_limits<int>::max()));
}

QString intervalUnitToString(IntervalUnit unit)
{
    switch (unit) {
    case IntervalUnit::Milliseconds:
        return QStringLiteral("milliseconds");
    case IntervalUnit::Seconds:
        return QStringLiteral("seconds");
    case IntervalUnit::Minutes:
        return QStringLiteral("minutes");
    case IntervalUnit::Hours:
        return QStringLiteral("hours");
    }

    return QStringLiteral("milliseconds");
}

QString mouseButtonToString(MouseButton button)
{
    switch (button) {
    case MouseButton::Left:
        return QStringLiteral("left");
    case MouseButton::Right:
        return QStringLiteral("right");
    case MouseButton::Middle:
        return QStringLiteral("middle");
    case MouseButton::CustomKey:
        return QStringLiteral("custom key");
    }

    return QStringLiteral("left");
}

QString clickTypeToString(ClickType type)
{
    switch (type) {
    case ClickType::Single:
        return QStringLiteral("single");
    case ClickType::Double:
        return QStringLiteral("double");
    }

    return QStringLiteral("single");
}

QString positionModeToString(PositionMode mode)
{
    switch (mode) {
    case PositionMode::CurrentCursor:
        return QStringLiteral("current cursor");
    case PositionMode::FixedCoordinate:
        return QStringLiteral("fixed coordinate");
    }

    return QStringLiteral("current cursor");
}

IntervalUnit intervalUnitFromInt(int value)
{
    switch (value) {
    case 1:
        return IntervalUnit::Seconds;
    case 2:
        return IntervalUnit::Minutes;
    case 3:
        return IntervalUnit::Hours;
    default:
        return IntervalUnit::Milliseconds;
    }
}

MouseButton mouseButtonFromInt(int value)
{
    switch (value) {
    case 1:
        return MouseButton::Right;
    case 2:
        return MouseButton::Middle;
    case 3:
        return MouseButton::CustomKey;
    default:
        return MouseButton::Left;
    }
}

ClickType clickTypeFromInt(int value)
{
    return value == 1 ? ClickType::Double : ClickType::Single;
}

PositionMode positionModeFromInt(int value)
{
    return value == 1 ? PositionMode::FixedCoordinate : PositionMode::CurrentCursor;
}
