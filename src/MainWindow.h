#pragma once

#include "ClickEngine.h"
#include "Config.h"
#include "Types.h"
#include "hotkeys/HotkeyManager.h"

#include <QElapsedTimer>
#include <QMainWindow>
#include <QVector>

class QCheckBox;
class QComboBox;
class QEvent;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QShortcut;
class QSpinBox;
class QSystemTrayIcon;
class QTabBar;
class QTabWidget;
class QToolButton;
class HotkeyCaptureEdit;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool shouldStartMinimized() const;
    void triggerHotkeyToggle();
    void triggerHotkeyToggle(const QString& hotkeyText);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct ProfileUi {
        ClickProfile profile;
        ClickEngine* engine = nullptr;
        QWidget* page = nullptr;
        QCheckBox* enabledCheck = nullptr;
        QPushButton* startStopButton = nullptr;
        QLabel* statusLabel = nullptr;
        QLabel* intervalSummaryLabel = nullptr;
        QLabel* buttonSummaryLabel = nullptr;
        QLabel* backendLabel = nullptr;
        QPlainTextEdit* limitationText = nullptr;
        QLabel* counterLabel = nullptr;
        QSpinBox* intervalSpin = nullptr;
        QComboBox* intervalUnitCombo = nullptr;
        QComboBox* buttonCombo = nullptr;
        QLabel* customKeyLabel = nullptr;
        HotkeyCaptureEdit* customKeyEdit = nullptr;
        QComboBox* clickTypeCombo = nullptr;
        QCheckBox* infiniteCheck = nullptr;
        QSpinBox* limitSpin = nullptr;
        QCheckBox* randomizeCheck = nullptr;
        QSpinBox* jitterSpin = nullptr;
        HotkeyCaptureEdit* hotkeyEdit = nullptr;
        QCheckBox* startMinimizedCheck = nullptr;
        QCheckBox* keepRunningInBackgroundCheck = nullptr;
        QPushButton* applyHotkeyButton = nullptr;
        QPushButton* resetButton = nullptr;
        QPushButton* donateButton = nullptr;
        QElapsedTimer hotkeyDebounce;
    };

    void buildUi();
    ProfileUi* addProfile(const ClickProfile& profile, bool makeCurrent);
    QWidget* buildProfilePage(ProfileUi* ui);
    QGroupBox* createClickOptionsGroup(ProfileUi* ui);
    QGroupBox* createHotkeyGroup(ProfileUi* ui);
    QGroupBox* createSettingsGroup(ProfileUi* ui);
    QGroupBox* createStatusGroup(ProfileUi* ui);
    void connectProfileSignals(ProfileUi* ui);
    void applyProfileToUi(ProfileUi* ui);
    ClickSettings settingsFromUi(const ProfileUi* ui) const;
    AppSettings appSettingsFromUi() const;
    void persistSettings();
    void refreshSummary(ProfileUi* ui);
    void refreshAllSummaries();
    void refreshBackendCapabilities(ProfileUi* ui);
    void refreshAllBackendCapabilities();
    void registerHotkeys();
    void applyHotkeyFromUi(ProfileUi* ui);
    void resetToDefaults(ProfileUi* ui);
    void startOrStop(ProfileUi* ui, bool delayManualStopButton);
    void setRunningUi(ProfileUi* ui, bool running);
    void setupTray();
    void updateTrayState();
    void updateTabText(ProfileUi* ui);
    void updateAllTabTexts();
    void updateTabCloseButtons();
    void updateTabSizing();
    void positionAddTabButton();
    void setFocusedShortcutsEnabled(bool enabled);
    void syncGlobalSettings(ProfileUi* source);
    void createNewProfile();
    void removeCurrentProfile();
    void removeProfile(ProfileUi* ui);
    bool validateCustomKey(ProfileUi* ui, bool interactive);
    ProfileUi* currentProfile() const;
    QVector<ProfileUi*> profilesForHotkey(const QString& hotkeyText) const;

    QVector<ProfileUi*> profiles_;
    QVector<QShortcut*> focusedShortcuts_;
    GlobalHotkeyManager hotkeys_;
    bool loading_ = false;
    bool startMinimized_ = false;
    bool keepRunningInBackground_ = false;
    int hoveredTabIndex_ = -1;
    int tabWidth_ = -1;

    QTabWidget* tabs_ = nullptr;
    QToolButton* addTabButton_ = nullptr;
    QSystemTrayIcon* trayIcon_ = nullptr;
};
