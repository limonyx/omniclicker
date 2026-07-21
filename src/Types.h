#pragma once

#include <QPoint>
#include <QString>
#include <QtGlobal>

enum class IntervalUnit {
    Milliseconds = 0,
    Seconds = 1,
    Minutes = 2,
    Hours = 3,
};

enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
    CustomKey = 3,
};

enum class ClickType {
    Single = 0,
    Double = 1,
};

enum class PositionMode {
    CurrentCursor = 0,
    FixedCoordinate = 1,
};

struct ClickSettings {
    int intervalValue = 100;
    IntervalUnit intervalUnit = IntervalUnit::Milliseconds;
    MouseButton mouseButton = MouseButton::Left;
    ClickType clickType = ClickType::Single;
    PositionMode positionMode = PositionMode::CurrentCursor;
    QPoint fixedPosition = QPoint(0, 0);
    QString customKey = QStringLiteral("Space");
    bool infinite = true;
    quint64 clickLimit = 100;
    bool randomizeInterval = false;
    int randomJitterPercent = 10;
    QString hotkey = QStringLiteral("F6");
    bool startMinimized = false;
    bool keepRunningInBackground = false;

    int intervalMilliseconds() const;
};

QString intervalUnitToString(IntervalUnit unit);
QString mouseButtonToString(MouseButton button);
QString clickTypeToString(ClickType type);
QString positionModeToString(PositionMode mode);

IntervalUnit intervalUnitFromInt(int value);
MouseButton mouseButtonFromInt(int value);
ClickType clickTypeFromInt(int value);
PositionMode positionModeFromInt(int value);
