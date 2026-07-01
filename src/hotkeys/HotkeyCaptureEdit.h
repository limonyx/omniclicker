#pragma once

#include <QLineEdit>

class HotkeyCaptureEdit final : public QLineEdit {
    Q_OBJECT

public:
    explicit HotkeyCaptureEdit(QWidget* parent = nullptr);

signals:
    void recordingChanged(bool recording);
    void hotkeyCaptured(const QString& hotkeyText);

protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
};
