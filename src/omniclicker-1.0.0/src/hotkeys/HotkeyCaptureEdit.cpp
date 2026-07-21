#include "hotkeys/HotkeyCaptureEdit.h"

#include "hotkeys/Hotkey.h"

#include <QFocusEvent>
#include <QKeyEvent>

HotkeyCaptureEdit::HotkeyCaptureEdit(QWidget* parent)
    : QLineEdit(parent)
{
    setPlaceholderText(QStringLiteral("Click here, press a shortcut, then Apply"));
    setClearButtonEnabled(false);
}

void HotkeyCaptureEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    emit recordingChanged(true);
    selectAll();
}

void HotkeyCaptureEdit::focusOutEvent(QFocusEvent* event)
{
    emit recordingChanged(false);
    QLineEdit::focusOutEvent(event);
}

void HotkeyCaptureEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        clearFocus();
        event->accept();
        return;
    }

    QString error;
    const std::optional<Hotkey> hotkey = hotkeyFromKeyEvent(event, &error);
    if (hotkey) {
        setText(hotkey->normalized);
        emit hotkeyCaptured(hotkey->normalized);
    }

    event->accept();
}
