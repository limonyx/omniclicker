#include "hotkeys/Hotkey.h"

#include <QStringList>

namespace {

bool isModifierKey(int key)
{
    switch (key) {
    case Qt::Key_Control:
    case Qt::Key_Shift:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
        return true;
    default:
        return false;
    }
}

QString keyToString(int key)
{
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        return QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
    }

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        return QChar(static_cast<char>('a' + key - Qt::Key_A));
    }

    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return QChar(static_cast<char>('0' + key - Qt::Key_0));
    }

    switch (key) {
    case Qt::Key_Space:
        return QStringLiteral("Space");
    case Qt::Key_Tab:
        return QStringLiteral("Tab");
    case Qt::Key_Backtab:
        return QStringLiteral("Backtab");
    case Qt::Key_Insert:
        return QStringLiteral("Insert");
    case Qt::Key_Delete:
        return QStringLiteral("Delete");
    case Qt::Key_Backspace:
        return QStringLiteral("Backspace");
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QStringLiteral("Enter");
    case Qt::Key_Escape:
        return QStringLiteral("Esc");
    case Qt::Key_Home:
        return QStringLiteral("Home");
    case Qt::Key_End:
        return QStringLiteral("End");
    case Qt::Key_PageUp:
        return QStringLiteral("PageUp");
    case Qt::Key_PageDown:
        return QStringLiteral("PageDown");
    case Qt::Key_Left:
        return QStringLiteral("Left");
    case Qt::Key_Right:
        return QStringLiteral("Right");
    case Qt::Key_Up:
        return QStringLiteral("Up");
    case Qt::Key_Down:
        return QStringLiteral("Down");
    case Qt::Key_Slash:
        return QStringLiteral("/");
    case Qt::Key_Backslash:
        return QStringLiteral("\\");
    case Qt::Key_Minus:
        return QStringLiteral("-");
    case Qt::Key_Equal:
        return QStringLiteral("=");
    case Qt::Key_Comma:
        return QStringLiteral(",");
    case Qt::Key_Period:
        return QStringLiteral(".");
    case Qt::Key_Semicolon:
        return QStringLiteral(";");
    case Qt::Key_Apostrophe:
        return QStringLiteral("'");
    case Qt::Key_BracketLeft:
        return QStringLiteral("[");
    case Qt::Key_BracketRight:
        return QStringLiteral("]");
    case Qt::Key_QuoteLeft:
        return QStringLiteral("`");
    default:
        return {};
    }
}

std::optional<int> parseKeyToken(const QString& token, Qt::KeyboardModifiers* implicitModifiers)
{
    const QString trimmed = token.trimmed();
    const QString upper = trimmed.toUpper();

    if (upper.startsWith(QStringLiteral("F"))) {
        bool ok = false;
        const int number = upper.mid(1).toInt(&ok);
        if (ok && number >= 1 && number <= 24) {
            return Qt::Key_F1 + number - 1;
        }
    }

    if (upper.size() == 1) {
        const QChar ch = upper.at(0);
        if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) {
            return Qt::Key_A + ch.unicode() - QLatin1Char('A').unicode();
        }
        if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9')) {
            return Qt::Key_0 + ch.unicode() - QLatin1Char('0').unicode();
        }

        switch (ch.unicode()) {
        case '/':
            return Qt::Key_Slash;
        case '?':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Slash;
        case '\\':
            return Qt::Key_Backslash;
        case '|':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Backslash;
        case '-':
            return Qt::Key_Minus;
        case '_':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Minus;
        case '=':
            return Qt::Key_Equal;
        case '+':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Equal;
        case ',':
            return Qt::Key_Comma;
        case '<':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Comma;
        case '.':
            return Qt::Key_Period;
        case '>':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Period;
        case ';':
            return Qt::Key_Semicolon;
        case ':':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Semicolon;
        case '\'':
            return Qt::Key_Apostrophe;
        case '"':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_Apostrophe;
        case '[':
            return Qt::Key_BracketLeft;
        case '{':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_BracketLeft;
        case ']':
            return Qt::Key_BracketRight;
        case '}':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_BracketRight;
        case '`':
            return Qt::Key_QuoteLeft;
        case '~':
            *implicitModifiers |= Qt::ShiftModifier;
            return Qt::Key_QuoteLeft;
        default:
            break;
        }
    }

    if (upper == QStringLiteral("SPACE")) {
        return Qt::Key_Space;
    }
    if (upper == QStringLiteral("TAB")) {
        return Qt::Key_Tab;
    }
    if (upper == QStringLiteral("BACKTAB")) {
        return Qt::Key_Backtab;
    }
    if (upper == QStringLiteral("INSERT") || upper == QStringLiteral("INS")) {
        return Qt::Key_Insert;
    }
    if (upper == QStringLiteral("DELETE") || upper == QStringLiteral("DEL")) {
        return Qt::Key_Delete;
    }
    if (upper == QStringLiteral("BACKSPACE")) {
        return Qt::Key_Backspace;
    }
    if (upper == QStringLiteral("ENTER") || upper == QStringLiteral("RETURN")) {
        return Qt::Key_Return;
    }
    if (upper == QStringLiteral("ESC") || upper == QStringLiteral("ESCAPE")) {
        return Qt::Key_Escape;
    }
    if (upper == QStringLiteral("HOME")) {
        return Qt::Key_Home;
    }
    if (upper == QStringLiteral("END")) {
        return Qt::Key_End;
    }
    if (upper == QStringLiteral("PAGEUP") || upper == QStringLiteral("PGUP")) {
        return Qt::Key_PageUp;
    }
    if (upper == QStringLiteral("PAGEDOWN") || upper == QStringLiteral("PGDN")) {
        return Qt::Key_PageDown;
    }
    if (upper == QStringLiteral("LEFT")) {
        return Qt::Key_Left;
    }
    if (upper == QStringLiteral("RIGHT")) {
        return Qt::Key_Right;
    }
    if (upper == QStringLiteral("UP")) {
        return Qt::Key_Up;
    }
    if (upper == QStringLiteral("DOWN")) {
        return Qt::Key_Down;
    }

    return std::nullopt;
}

int canonicalKeyFromEvent(QKeyEvent* event, Qt::KeyboardModifiers* modifiers)
{
    int key = event->key();
    const QString text = event->text();

    if (text.size() == 1) {
        Qt::KeyboardModifiers implicitModifiers = Qt::NoModifier;
        const std::optional<int> parsed = parseKeyToken(text, &implicitModifiers);
        if (parsed) {
            key = *parsed;
            *modifiers |= implicitModifiers;
        }
    }

    switch (key) {
    case Qt::Key_Question:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Slash;
    case Qt::Key_Plus:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Equal;
    case Qt::Key_Underscore:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Minus;
    case Qt::Key_Bar:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Backslash;
    case Qt::Key_Less:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Comma;
    case Qt::Key_Greater:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Period;
    case Qt::Key_Colon:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Semicolon;
    case Qt::Key_QuoteDbl:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_Apostrophe;
    case Qt::Key_BraceLeft:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_BracketLeft;
    case Qt::Key_BraceRight:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_BracketRight;
    case Qt::Key_AsciiTilde:
        *modifiers |= Qt::ShiftModifier;
        return Qt::Key_QuoteLeft;
    default:
        return key;
    }
}

} // namespace

std::optional<Hotkey> parseHotkey(const QString& text, QString* error)
{
    const QStringList parts = text.split(QLatin1Char('+'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Hotkey cannot be empty.");
        }
        return std::nullopt;
    }

    Hotkey hotkey;
    hotkey.modifiers = Qt::NoModifier;
    std::optional<int> key;

    for (const QString& rawPart : parts) {
        const QString part = rawPart.trimmed();
        const QString upper = part.toUpper();
        if (upper == QStringLiteral("CTRL") || upper == QStringLiteral("CONTROL")) {
            hotkey.modifiers |= Qt::ControlModifier;
        } else if (upper == QStringLiteral("ALT")) {
            hotkey.modifiers |= Qt::AltModifier;
        } else if (upper == QStringLiteral("SHIFT")) {
            hotkey.modifiers |= Qt::ShiftModifier;
        } else if (upper == QStringLiteral("SUPER") || upper == QStringLiteral("META") || upper == QStringLiteral("WIN")) {
            hotkey.modifiers |= Qt::MetaModifier;
        } else if (!key) {
            key = parseKeyToken(part, &hotkey.modifiers);
        } else {
            if (error) {
                *error = QStringLiteral("Hotkey contains more than one non-modifier key.");
            }
            return std::nullopt;
        }
    }

    if (!key) {
        if (error) {
            *error = QStringLiteral("Hotkey must include a key, for example F6 or Ctrl + F6.");
        }
        return std::nullopt;
    }

    hotkey.key = *key;
    hotkey.normalized = normalizeHotkey(hotkey.key, hotkey.modifiers);
    return hotkey;
}

std::optional<Hotkey> hotkeyFromKeyEvent(QKeyEvent* event, QString* error)
{
    Qt::KeyboardModifiers modifiers = event->modifiers()
        & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    const int key = canonicalKeyFromEvent(event, &modifiers);

    if (isModifierKey(key)) {
        if (error) {
            *error = QStringLiteral("Press a non-modifier key with the desired modifiers.");
        }
        return std::nullopt;
    }

    if (key == Qt::Key_unknown || keyToString(key).isEmpty()) {
        if (error) {
            *error = QStringLiteral("Unsupported hotkey key.");
        }
        return std::nullopt;
    }

    Hotkey hotkey;
    hotkey.key = key;
    hotkey.modifiers = modifiers;
    hotkey.normalized = normalizeHotkey(hotkey.key, hotkey.modifiers);
    return hotkey;
}

QString normalizeHotkey(int key, Qt::KeyboardModifiers modifiers)
{
    QStringList parts;
    if (modifiers.testFlag(Qt::ControlModifier)) {
        parts << QStringLiteral("Ctrl");
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        parts << QStringLiteral("Alt");
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        parts << QStringLiteral("Shift");
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        parts << QStringLiteral("Super");
    }

    const QString keyText = keyToString(key);
    if (!keyText.isEmpty()) {
        parts << keyText;
    }

    return parts.join(QStringLiteral(" + "));
}

QKeySequence hotkeyToKeySequence(const Hotkey& hotkey)
{
    return QKeySequence(hotkey.modifiers.toInt() | hotkey.key);
}
