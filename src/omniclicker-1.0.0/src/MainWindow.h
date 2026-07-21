#pragma once

#include "ClickEngine.h"
#include "Types.h"
#include "hotkeys/HotkeyManager.h"

#include <QMainWindow>
#include <QElapsedTimer>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QShortcut;
class QSpinBox;
class QSystemTrayIcon;
class HotkeyCaptureEdit;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool shouldStartMinimized() const;
    void triggerHotkeyToggle();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    QGroupBox* createClickOptionsGroup();
    QGroupBox* createHotkeyGroup();
    QGroupBox* createSettingsGroup();
    QGroupBox* createStatusGroup();
    void connectSignals();
    void applySettingsToUi();
    ClickSettings settingsFromUi() const;
    void persistSettings();
    void refreshSummary();
    void refreshBackendCapabilities();
    void registerHotkey();
    void applyHotkeyFromUi();
    void resetToDefaults();
    void startOrStop(bool delayManualStopButton);
    void setRunningUi(bool running);
    void setupTray();
    void updateTrayState();
    bool validateCustomKey(bool interactive);

    ClickSettings settings_;
    ClickEngine engine_;
    GlobalHotkeyManager hotkeys_;
    bool loading_ = false;

    QLabel* statusLabel_ = nullptr;
    QLabel* intervalSummaryLabel_ = nullptr;
    QLabel* buttonSummaryLabel_ = nullptr;
    QLabel* backendLabel_ = nullptr;
    QPlainTextEdit* limitationText_ = nullptr;
    QLabel* counterLabel_ = nullptr;
    QSpinBox* intervalSpin_ = nullptr;
    QComboBox* intervalUnitCombo_ = nullptr;
    QComboBox* buttonCombo_ = nullptr;
    QLabel* customKeyLabel_ = nullptr;
    HotkeyCaptureEdit* customKeyEdit_ = nullptr;
    QComboBox* clickTypeCombo_ = nullptr;
    QCheckBox* infiniteCheck_ = nullptr;
    QSpinBox* limitSpin_ = nullptr;
    QCheckBox* randomizeCheck_ = nullptr;
    QSpinBox* jitterSpin_ = nullptr;
    HotkeyCaptureEdit* hotkeyEdit_ = nullptr;
    QCheckBox* startMinimizedCheck_ = nullptr;
    QCheckBox* keepRunningInBackgroundCheck_ = nullptr;
    QPushButton* applyHotkeyButton_ = nullptr;
    QPushButton* resetButton_ = nullptr;
    QPushButton* donateButton_ = nullptr;
    QShortcut* focusedShortcut_ = nullptr;
    QPushButton* startStopButton_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
    QElapsedTimer hotkeyDebounce_;
};
