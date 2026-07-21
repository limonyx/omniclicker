#pragma once

#include <QKeyEvent>
#include <QKeySequence>
#include <QString>
#include <Qt>

#include <optional>

struct Hotkey {
    int key = Qt::Key_F6;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    QString normalized = QStringLiteral("F6");
};

std::optional<Hotkey> parseHotkey(const QString& text, QString* error = nullptr);
std::optional<Hotkey> hotkeyFromKeyEvent(QKeyEvent* event, QString* error = nullptr);
QString normalizeHotkey(int key, Qt::KeyboardModifiers modifiers);
QKeySequence hotkeyToKeySequence(const Hotkey& hotkey);
