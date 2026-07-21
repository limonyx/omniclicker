#include "MainWindow.h"

#include "backends/InputBackend.h"
#include "hotkeys/Hotkey.h"
#include "hotkeys/HotkeyCaptureEdit.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "unknown"
#endif

#include <algorithm>
#include <limits>

namespace {

constexpr int MaximumProfiles = 8;

int maximumIntervalValue(IntervalUnit unit)
{
    switch (unit) {
    case IntervalUnit::Milliseconds:
        return 999999;
    case IntervalUnit::Seconds:
        return std::numeric_limits<int>::max() / 1000;
    case IntervalUnit::Minutes:
        return std::numeric_limits<int>::max() / (60 * 1000);
    case IntervalUnit::Hours:
        return std::numeric_limits<int>::max() / (60 * 60 * 1000);
    }

    return 999999;
}

void addComboItem(QComboBox* combo, const QString& label, int value)
{
    combo->addItem(label, value);
}

int comboValue(const QComboBox* combo)
{
    return combo->currentData().toInt();
}

void setComboValue(QComboBox* combo, int value)
{
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    return label;
}

QString intervalSummary(const ClickSettings& settings)
{
    return QStringLiteral("%1 %2").arg(settings.intervalValue).arg(intervalUnitToString(settings.intervalUnit));
}

QString tabIntervalSummary(const ClickSettings& settings)
{
    QString unit;
    switch (settings.intervalUnit) {
    case IntervalUnit::Milliseconds:
        unit = QStringLiteral("ms");
        break;
    case IntervalUnit::Seconds:
        unit = QStringLiteral("s");
        break;
    case IntervalUnit::Minutes:
        unit = QStringLiteral("min");
        break;
    case IntervalUnit::Hours:
        unit = QStringLiteral("h");
        break;
    }
    return QStringLiteral("%1 %2").arg(settings.intervalValue).arg(unit);
}

QString buttonSummary(const ClickSettings& settings)
{
    if (settings.mouseButton == MouseButton::CustomKey) {
        return QStringLiteral("key %1").arg(settings.customKey);
    }
    return QStringLiteral("%1, %2").arg(mouseButtonToString(settings.mouseButton), clickTypeToString(settings.clickType));
}

QString tabSummary(const ClickSettings& settings)
{
    return QStringLiteral("%1 | %2 | %3")
        .arg(settings.hotkey, mouseButtonToString(settings.mouseButton), tabIntervalSummary(settings));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    const AppSettings appSettings = AppConfig::loadAppSettings();
    startMinimized_ = appSettings.startMinimized;
    keepRunningInBackground_ = appSettings.keepRunningInBackground;

    buildUi();
    setupTray();
    connect(&hotkeys_, &GlobalHotkeyManager::hotkeyActivated, this, [this](const QString& hotkeyText) {
        triggerHotkeyToggle(hotkeyText);
    });
    connect(&hotkeys_, &GlobalHotkeyManager::errorOccurred, this, [this](const QString& message) {
        statusBar()->showMessage(message, 8000);
        if (ProfileUi* ui = currentProfile()) {
            ui->limitationText->setPlainText(message);
        }
    });
    connect(&hotkeys_, &GlobalHotkeyManager::statusChanged, this, [this](const QString& message) {
        statusBar()->showMessage(message, 5000);
        if (ProfileUi* ui = currentProfile()) {
            ui->limitationText->setPlainText(message);
        }
    });

    loading_ = true;
    QVector<ClickProfile> profiles = appSettings.profiles;
    if (profiles.isEmpty()) {
        profiles.append(ClickProfile {});
    }
    for (const ClickProfile& profile : profiles) {
        if (profiles_.size() == MaximumProfiles) {
            break;
        }
        addProfile(profile, profiles_.isEmpty());
    }
    loading_ = false;

    refreshAllSummaries();
    refreshAllBackendCapabilities();
    registerHotkeys();
    QTimer::singleShot(0, this, &MainWindow::updateTabSizing);
}

MainWindow::~MainWindow()
{
    hotkeys_.stop();
    for (ProfileUi* ui : profiles_) {
        ui->engine->stop();
    }
    qDeleteAll(profiles_);
}

bool MainWindow::shouldStartMinimized() const
{
    return startMinimized_;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    persistSettings();
    if (keepRunningInBackground_ && trayIcon_ && trayIcon_->isVisible()) {
        hide();
        event->ignore();
        trayIcon_->showMessage(QStringLiteral("Omni Clicker"),
                               QStringLiteral("Omni Clicker is still running in the background."),
                               QSystemTrayIcon::Information,
                               2500);
        return;
    }

    for (ProfileUi* ui : profiles_) {
        ui->engine->stop();
    }
    hotkeys_.stop();
    event->accept();
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Omni Clicker"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.omniclicker")));
    resize(760, 540);
    setMinimumSize(680, 480);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(10);

    tabs_ = new QTabWidget(central);
    tabs_->setDocumentMode(true);
    tabs_->setMovable(true);
    tabs_->tabBar()->setElideMode(Qt::ElideRight);
    tabs_->tabBar()->setUsesScrollButtons(false);
    tabs_->tabBar()->setMouseTracking(true);
    tabs_->tabBar()->installEventFilter(this);
    addTabButton_ = new QToolButton(tabs_->tabBar());
    addTabButton_->setText(QStringLiteral("+"));
    addTabButton_->setToolTip(QStringLiteral("Add tab"));
    addTabButton_->setAutoRaise(true);
    addTabButton_->setFixedSize(26, 26);
    addTabButton_->setStyleSheet(QStringLiteral(
        "QToolButton { border: 0; border-radius: 13px; font-size: 18px; }"
        "QToolButton:hover { background-color: palette(midlight); }"));
    addTabButton_->show();
    root->addWidget(tabs_, 1);

    connect(addTabButton_, &QToolButton::clicked, this, &MainWindow::createNewProfile);
    connect(tabs_, &QTabWidget::currentChanged, this, [this]() {
        updateTrayState();
        updateTabCloseButtons();
    });
    connect(tabs_->tabBar(), &QTabBar::tabMoved, this, [this]() {
        positionAddTabButton();
    });

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Stopped"));

    auto* buildInfoLabel = new QLabel(QStringLiteral("v" APP_VERSION " (" BUILD_TIMESTAMP ")"));
    buildInfoLabel->setStyleSheet(QStringLiteral("color: #888;"));
    statusBar()->addPermanentWidget(buildInfoLabel);
}

MainWindow::ProfileUi* MainWindow::addProfile(const ClickProfile& profile, bool makeCurrent)
{
    auto* ui = new ProfileUi;
    ui->profile = profile;
    ui->profile.settings.startMinimized = startMinimized_;
    ui->profile.settings.keepRunningInBackground = keepRunningInBackground_;
    ui->engine = new ClickEngine(this);
    ui->page = buildProfilePage(ui);
    profiles_.append(ui);

    applyProfileToUi(ui);
    connectProfileSignals(ui);

    const int index = tabs_->addTab(ui->page, tabSummary(ui->profile.settings));
    if (makeCurrent) {
        tabs_->setCurrentIndex(index);
    }
    updateTabCloseButtons();
    addTabButton_->setVisible(profiles_.size() < MaximumProfiles);
    updateTabSizing();
    positionAddTabButton();
    return ui;
}

QWidget* MainWindow::buildProfilePage(ProfileUi* ui)
{
    auto* page = new QWidget(tabs_);
    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(0, 12, 0, 0);
    root->setSpacing(12);

    auto* header = new QHBoxLayout();
    ui->enabledCheck = new QCheckBox(QStringLiteral("Enabled"), page);
    ui->enabledCheck->setToolTip(QStringLiteral("Disabled tabs ignore hotkey activation."));
    ui->startStopButton = new QPushButton(QStringLiteral("Start clicking"), page);
    ui->startStopButton->setObjectName(QStringLiteral("PrimaryButton"));
    ui->startStopButton->setMinimumHeight(42);
    ui->startStopButton->setMinimumWidth(170);
    header->addWidget(ui->enabledCheck);
    header->addStretch();
    header->addWidget(ui->startStopButton);
    root->addLayout(header);

    auto* content = new QHBoxLayout();
    content->setSpacing(12);

    auto* leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(12);
    leftColumn->addWidget(createClickOptionsGroup(ui));
    leftColumn->addWidget(createSettingsGroup(ui));
    leftColumn->addStretch();
    ui->donateButton = new QPushButton(QStringLiteral("Donate"), page);
    leftColumn->addWidget(ui->donateButton, 0, Qt::AlignLeft);

    auto* rightColumn = new QVBoxLayout();
    rightColumn->setSpacing(12);
    rightColumn->addWidget(createStatusGroup(ui));
    rightColumn->addWidget(createHotkeyGroup(ui));
    rightColumn->addStretch();
    auto* applyRow = new QHBoxLayout();
    applyRow->addStretch();
    ui->resetButton = new QPushButton(QStringLiteral("Reset"), page);
    applyRow->addWidget(ui->resetButton);
    applyRow->addWidget(ui->applyHotkeyButton);
    rightColumn->addLayout(applyRow);

    content->addLayout(leftColumn, 3);
    content->addLayout(rightColumn, 2);
    root->addLayout(content, 1);
    return page;
}

QGroupBox* MainWindow::createClickOptionsGroup(ProfileUi* ui)
{
    auto* group = new QGroupBox(QStringLiteral("Click options"), ui->page);
    auto* layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);

    ui->intervalSpin = new QSpinBox(group);
    ui->intervalSpin->setRange(1, 999999);
    ui->intervalSpin->setMinimumWidth(92);

    ui->intervalUnitCombo = new QComboBox(group);
    addComboItem(ui->intervalUnitCombo, QStringLiteral("milliseconds"), static_cast<int>(IntervalUnit::Milliseconds));
    addComboItem(ui->intervalUnitCombo, QStringLiteral("seconds"), static_cast<int>(IntervalUnit::Seconds));
    addComboItem(ui->intervalUnitCombo, QStringLiteral("minutes"), static_cast<int>(IntervalUnit::Minutes));
    addComboItem(ui->intervalUnitCombo, QStringLiteral("hours"), static_cast<int>(IntervalUnit::Hours));

    layout->addWidget(fieldLabel(QStringLiteral("Interval"), group), 0, 0);
    layout->addWidget(ui->intervalSpin, 0, 1);
    layout->addWidget(ui->intervalUnitCombo, 0, 2, 1, 2);

    ui->buttonCombo = new QComboBox(group);
    addComboItem(ui->buttonCombo, QStringLiteral("Left click"), static_cast<int>(MouseButton::Left));
    addComboItem(ui->buttonCombo, QStringLiteral("Right click"), static_cast<int>(MouseButton::Right));
    addComboItem(ui->buttonCombo, QStringLiteral("Middle click"), static_cast<int>(MouseButton::Middle));
    addComboItem(ui->buttonCombo, QStringLiteral("Custom key"), static_cast<int>(MouseButton::CustomKey));

    ui->customKeyEdit = new HotkeyCaptureEdit(group);
    ui->customKeyEdit->setPlaceholderText(QStringLiteral("Press the key to repeat"));
    ui->customKeyEdit->setToolTip(QStringLiteral("This key is pressed at each interval while Omni Clicker is running."));

    ui->clickTypeCombo = new QComboBox(group);
    addComboItem(ui->clickTypeCombo, QStringLiteral("Single"), static_cast<int>(ClickType::Single));
    addComboItem(ui->clickTypeCombo, QStringLiteral("Double"), static_cast<int>(ClickType::Double));

    layout->addWidget(fieldLabel(QStringLiteral("Button"), group), 1, 0);
    layout->addWidget(ui->buttonCombo, 1, 1);
    layout->addWidget(fieldLabel(QStringLiteral("Type"), group), 1, 2);
    layout->addWidget(ui->clickTypeCombo, 1, 3);

    ui->customKeyLabel = fieldLabel(QStringLiteral("Custom key"), group);
    layout->addWidget(ui->customKeyLabel, 2, 0);
    layout->addWidget(ui->customKeyEdit, 2, 1, 1, 3);

    ui->infiniteCheck = new QCheckBox(QStringLiteral("Run until stopped"), group);
    ui->limitSpin = new QSpinBox(group);
    ui->limitSpin->setRange(1, 1000000000);

    layout->addWidget(ui->infiniteCheck, 3, 0, 1, 2);
    layout->addWidget(fieldLabel(QStringLiteral("Limit"), group), 3, 2);
    layout->addWidget(ui->limitSpin, 3, 3);

    ui->randomizeCheck = new QCheckBox(QStringLiteral("Randomize"), group);
    ui->jitterSpin = new QSpinBox(group);
    ui->jitterSpin->setRange(1, 95);
    ui->jitterSpin->setSuffix(QStringLiteral("%"));

    layout->addWidget(ui->randomizeCheck, 4, 0, 1, 2);
    layout->addWidget(fieldLabel(QStringLiteral("Jitter"), group), 4, 2);
    layout->addWidget(ui->jitterSpin, 4, 3);

    return group;
}

QGroupBox* MainWindow::createHotkeyGroup(ProfileUi* ui)
{
    auto* group = new QGroupBox(QStringLiteral("Hotkey"), ui->page);
    auto* layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);
    layout->setColumnStretch(1, 1);

    ui->hotkeyEdit = new HotkeyCaptureEdit(group);
    ui->hotkeyEdit->setToolTip(QStringLiteral("Click here, press the shortcut, then click Apply."));
    ui->applyHotkeyButton = new QPushButton(QStringLiteral("Apply"), ui->page);
    layout->addWidget(fieldLabel(QStringLiteral("Toggle"), group), 0, 0);
    layout->addWidget(ui->hotkeyEdit, 0, 1);

    return group;
}

QGroupBox* MainWindow::createSettingsGroup(ProfileUi* ui)
{
    auto* group = new QGroupBox(QStringLiteral("Settings"), ui->page);
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    ui->startMinimizedCheck = new QCheckBox(QStringLiteral("Start minimized"), group);
    ui->startMinimizedCheck->setToolTip(QStringLiteral("Start as a minimized taskbar window instead of opening in the foreground."));

    ui->keepRunningInBackgroundCheck = new QCheckBox(QStringLiteral("Keep running in background with tray icon"), group);
    ui->keepRunningInBackgroundCheck->setToolTip(QStringLiteral("When enabled, closing the window hides Omni Clicker to the tray instead of quitting."));

    layout->addWidget(ui->startMinimizedCheck);
    layout->addWidget(ui->keepRunningInBackgroundCheck);
    return group;
}

QGroupBox* MainWindow::createStatusGroup(ProfileUi* ui)
{
    auto* group = new QGroupBox(QStringLiteral("Status"), ui->page);
    auto* layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);

    ui->statusLabel = new QLabel(QStringLiteral("Stopped"), group);
    ui->intervalSummaryLabel = new QLabel(group);
    ui->buttonSummaryLabel = new QLabel(group);
    ui->counterLabel = new QLabel(QStringLiteral("0"), group);
    ui->backendLabel = new QLabel(QStringLiteral("Detecting..."), group);
    ui->backendLabel->setWordWrap(true);
    ui->backendLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    ui->limitationText = new QPlainTextEdit(group);
    ui->limitationText->setReadOnly(true);
    ui->limitationText->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    ui->limitationText->setMinimumHeight(90);
    ui->limitationText->setMaximumHeight(130);
    ui->limitationText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->addWidget(fieldLabel(QStringLiteral("State:"), group), 0, 0);
    layout->addWidget(ui->statusLabel, 0, 1);
    layout->addWidget(fieldLabel(QStringLiteral("Clicks"), group), 0, 2);
    layout->addWidget(ui->counterLabel, 0, 3);
    layout->addWidget(fieldLabel(QStringLiteral("Interval:"), group), 1, 0);
    layout->addWidget(ui->intervalSummaryLabel, 1, 1);
    layout->addWidget(fieldLabel(QStringLiteral("Button"), group), 1, 2);
    layout->addWidget(ui->buttonSummaryLabel, 1, 3);
    layout->addWidget(fieldLabel(QStringLiteral("Backend:"), group), 2, 0);
    layout->addWidget(ui->backendLabel, 2, 1, 1, 3);
    layout->addWidget(fieldLabel(QStringLiteral("Notes:"), group), 3, 0, Qt::AlignTop);
    layout->addWidget(ui->limitationText, 3, 1, 1, 3);
    return group;
}

void MainWindow::connectProfileSignals(ProfileUi* ui)
{
    const auto persist = [this, ui]() {
        ui->profile.settings = settingsFromUi(ui);
        persistSettings();
    };

    connect(ui->enabledCheck, &QCheckBox::toggled, this, [this, ui](bool enabled) {
        ui->profile.enabled = enabled;
        if (!enabled && ui->engine->isRunning()) {
            ui->engine->stop();
        }
        persistSettings();
        registerHotkeys();
        updateTabText(ui);
    });
    connect(ui->intervalUnitCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [ui]() {
        ui->intervalSpin->setMaximum(maximumIntervalValue(intervalUnitFromInt(comboValue(ui->intervalUnitCombo))));
    });
    connect(ui->intervalSpin, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(ui->intervalUnitCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, persist);
    connect(ui->buttonCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, persist);
    connect(ui->buttonCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, ui]() {
        refreshSummary(ui);
    });
    connect(ui->customKeyEdit, &HotkeyCaptureEdit::hotkeyCaptured, this, [this, ui](const QString&) {
        validateCustomKey(ui, true);
        ui->profile.settings = settingsFromUi(ui);
        persistSettings();
    });
    connect(ui->customKeyEdit, &HotkeyCaptureEdit::recordingChanged, this, [this](bool recording) {
        if (recording) {
            hotkeys_.stop();
            setFocusedShortcutsEnabled(false);
            return;
        }
        registerHotkeys();
    });
    connect(ui->clickTypeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, persist);
    connect(ui->infiniteCheck, &QCheckBox::toggled, this, persist);
    connect(ui->limitSpin, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(ui->randomizeCheck, &QCheckBox::toggled, this, persist);
    connect(ui->jitterSpin, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(ui->startMinimizedCheck, &QCheckBox::toggled, this, [this, ui](bool enabled) {
        startMinimized_ = enabled;
        syncGlobalSettings(ui);
        persistSettings();
    });
    connect(ui->keepRunningInBackgroundCheck, &QCheckBox::toggled, this, [this, ui](bool enabled) {
        keepRunningInBackground_ = enabled;
        syncGlobalSettings(ui);
        persistSettings();
        if (trayIcon_) {
            trayIcon_->setVisible(enabled);
        }
    });
    connect(ui->hotkeyEdit, &HotkeyCaptureEdit::recordingChanged, this, [this, ui](bool recording) {
        setFocusedShortcutsEnabled(!recording);
        if (recording) {
            hotkeys_.stop();
            return;
        }

        QTimer::singleShot(0, this, [this, ui]() {
            if (QApplication::focusWidget() != ui->applyHotkeyButton) {
                registerHotkeys();
            }
        });
    });
    connect(ui->hotkeyEdit, &HotkeyCaptureEdit::hotkeyCaptured, this, [this](const QString& hotkeyText) {
        statusBar()->showMessage(QStringLiteral("Pending hotkey: %1. Click Apply to save it.").arg(hotkeyText), 4000);
    });
    connect(ui->applyHotkeyButton, &QPushButton::clicked, this, [this, ui]() {
        applyHotkeyFromUi(ui);
    });
    connect(ui->resetButton, &QPushButton::clicked, this, [this, ui]() {
        resetToDefaults(ui);
    });
    connect(ui->donateButton, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://ko-fi.com/limonyx")));
    });
    connect(ui->startStopButton, &QPushButton::clicked, this, [this, ui]() {
        startOrStop(ui, true);
    });

    connect(ui->engine, &ClickEngine::runningChanged, this, [this, ui](bool running) {
        setRunningUi(ui, running);
    });
    connect(ui->engine, &ClickEngine::clickCountChanged, this, [this, ui](quint64 count) {
        ui->counterLabel->setText(QString::number(count));
        updateTrayState();
    });
    connect(ui->engine, &ClickEngine::backendChanged, this, [ui](const QString& name, const QString& details) {
        ui->backendLabel->setText(name);
        ui->limitationText->setPlainText(details.isEmpty() ? QStringLiteral("Full feature set available.") : details);
    });
    connect(ui->engine, &ClickEngine::errorOccurred, this, [this](const QString& message) {
        statusBar()->showMessage(message, 8000);
        QMessageBox::warning(this, QStringLiteral("Autoclicker error"), message);
    });
}

void MainWindow::applyProfileToUi(ProfileUi* ui)
{
    ui->enabledCheck->setChecked(ui->profile.enabled);
    setComboValue(ui->intervalUnitCombo, static_cast<int>(ui->profile.settings.intervalUnit));
    ui->intervalSpin->setMaximum(maximumIntervalValue(ui->profile.settings.intervalUnit));
    ui->intervalSpin->setValue(ui->profile.settings.intervalValue);
    setComboValue(ui->buttonCombo, static_cast<int>(ui->profile.settings.mouseButton));
    setComboValue(ui->clickTypeCombo, static_cast<int>(ui->profile.settings.clickType));
    ui->customKeyEdit->setText(ui->profile.settings.customKey);
    ui->infiniteCheck->setChecked(ui->profile.settings.infinite);
    ui->limitSpin->setValue(static_cast<int>(std::min<quint64>(ui->profile.settings.clickLimit, 1000000000ULL)));
    ui->randomizeCheck->setChecked(ui->profile.settings.randomizeInterval);
    ui->jitterSpin->setValue(ui->profile.settings.randomJitterPercent);
    ui->hotkeyEdit->setText(ui->profile.settings.hotkey);
    ui->startMinimizedCheck->setChecked(startMinimized_);
    ui->keepRunningInBackgroundCheck->setChecked(keepRunningInBackground_);
    if (trayIcon_) {
        trayIcon_->setVisible(keepRunningInBackground_);
    }
}

ClickSettings MainWindow::settingsFromUi(const ProfileUi* ui) const
{
    ClickSettings settings;
    settings.intervalValue = ui->intervalSpin->value();
    settings.intervalUnit = intervalUnitFromInt(comboValue(ui->intervalUnitCombo));
    settings.mouseButton = mouseButtonFromInt(comboValue(ui->buttonCombo));
    settings.clickType = clickTypeFromInt(comboValue(ui->clickTypeCombo));
    settings.positionMode = PositionMode::CurrentCursor;
    settings.fixedPosition = QPoint(0, 0);
    settings.customKey = ui->customKeyEdit->text().trimmed();
    settings.infinite = ui->infiniteCheck->isChecked();
    settings.clickLimit = static_cast<quint64>(ui->limitSpin->value());
    settings.randomizeInterval = ui->randomizeCheck->isChecked();
    settings.randomJitterPercent = ui->jitterSpin->value();
    settings.hotkey = ui->profile.settings.hotkey;
    settings.startMinimized = startMinimized_;
    settings.keepRunningInBackground = keepRunningInBackground_;
    return settings;
}

AppSettings MainWindow::appSettingsFromUi() const
{
    AppSettings result;
    result.startMinimized = startMinimized_;
    result.keepRunningInBackground = keepRunningInBackground_;
    for (ProfileUi* ui : profiles_) {
        ClickProfile profile = ui->profile;
        profile.settings = settingsFromUi(ui);
        result.profiles.append(profile);
    }
    return result;
}

void MainWindow::persistSettings()
{
    if (loading_) {
        return;
    }

    for (ProfileUi* ui : profiles_) {
        ui->profile.settings = settingsFromUi(ui);
        if (ui->profile.settings.mouseButton == MouseButton::CustomKey && !validateCustomKey(ui, false)) {
            return;
        }
        refreshSummary(ui);
    }

    QString error;
    if (!AppConfig::saveAppSettings(appSettingsFromUi(), &error)) {
        statusBar()->showMessage(error, 8000);
    }
}

void MainWindow::refreshSummary(ProfileUi* ui)
{
    const ClickSettings current = settingsFromUi(ui);
    ui->intervalSummaryLabel->setText(intervalSummary(current));
    const bool customKey = current.mouseButton == MouseButton::CustomKey;
    ui->customKeyLabel->setVisible(customKey);
    ui->customKeyEdit->setEnabled(customKey);
    ui->customKeyEdit->setVisible(customKey);
    ui->buttonSummaryLabel->setText(buttonSummary(current));
    ui->limitSpin->setEnabled(!current.infinite);
    ui->jitterSpin->setEnabled(current.randomizeInterval);
    updateTabText(ui);
    updateTrayState();
}

void MainWindow::refreshAllSummaries()
{
    for (ProfileUi* ui : profiles_) {
        refreshSummary(ui);
    }
}

void MainWindow::refreshBackendCapabilities(ProfileUi* ui)
{
    QString error;
    std::unique_ptr<InputBackend> backend = createInputBackend(&error);
    if (!backend) {
        ui->backendLabel->setText(QStringLiteral("Unavailable"));
        ui->limitationText->setPlainText(error);
        return;
    }

    if (!backend->initialize(&error)) {
        ui->backendLabel->setText(backend->name());
        ui->limitationText->setPlainText(error);
        return;
    }

    const BackendCapabilities caps = backend->capabilities();
    ui->backendLabel->setText(backend->name());
    ui->limitationText->setPlainText(caps.limitations.isEmpty() ? QStringLiteral("Full feature set available.") : caps.limitations.join(QStringLiteral("\n")));
}

void MainWindow::refreshAllBackendCapabilities()
{
    for (ProfileUi* ui : profiles_) {
        refreshBackendCapabilities(ui);
    }
}

void MainWindow::registerHotkeys()
{
    hotkeys_.stop();
    qDeleteAll(focusedShortcuts_);
    focusedShortcuts_.clear();

    QStringList hotkeyTexts;
    for (ProfileUi* ui : profiles_) {
        if (!ui->profile.enabled) {
            continue;
        }

        QString error;
        const std::optional<Hotkey> parsed = parseHotkey(ui->profile.settings.hotkey, &error);
        if (!parsed) {
            ui->limitationText->setPlainText(error);
            continue;
        }

        ui->profile.settings.hotkey = parsed->normalized;
        ui->hotkeyEdit->setText(parsed->normalized);
        if (!hotkeyTexts.contains(parsed->normalized)) {
            hotkeyTexts << parsed->normalized;
            auto* shortcut = new QShortcut(hotkeyToKeySequence(*parsed), this);
            shortcut->setContext(Qt::ApplicationShortcut);
            connect(shortcut, &QShortcut::activated, this, [this, normalized = parsed->normalized]() {
                triggerHotkeyToggle(normalized);
            });
            connect(shortcut, &QShortcut::activatedAmbiguously, this, [this, normalized = parsed->normalized]() {
                triggerHotkeyToggle(normalized);
            });
            focusedShortcuts_.append(shortcut);
        }
    }

    if (hotkeyTexts.isEmpty()) {
        return;
    }

    hotkeys_.setHotkeyTexts(hotkeyTexts);
    updateAllTabTexts();
}

void MainWindow::applyHotkeyFromUi(ProfileUi* ui)
{
    QString error;
    const std::optional<Hotkey> parsed = parseHotkey(ui->hotkeyEdit->text(), &error);
    if (!parsed) {
        statusBar()->showMessage(error, 5000);
        QMessageBox::warning(this, QStringLiteral("Invalid hotkey"), error);
        return;
    }

    ui->profile.settings.hotkey = parsed->normalized;
    if (!validateCustomKey(ui, true)) {
        return;
    }
    ui->hotkeyEdit->setText(ui->profile.settings.hotkey);
    persistSettings();
    registerHotkeys();
    ui->hotkeyEdit->clearFocus();
    setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(QStringLiteral("Hotkey applied: %1").arg(ui->profile.settings.hotkey), 4000);
}

void MainWindow::resetToDefaults(ProfileUi* ui)
{
    if (ui->engine->isRunning()) {
        ui->engine->stop();
    }

    ui->profile = ClickProfile {};
    loading_ = true;
    applyProfileToUi(ui);
    loading_ = false;
    refreshSummary(ui);
    persistSettings();
    registerHotkeys();
    ui->hotkeyEdit->clearFocus();
    setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(QStringLiteral("Tab reset to defaults."), 4000);
}

void MainWindow::startOrStop(ProfileUi* ui, bool delayManualStopButton)
{
    persistSettings();
    if (!ui->profile.enabled) {
        statusBar()->showMessage(QStringLiteral("This tab is disabled."), 4000);
        return;
    }
    if (ui->profile.settings.mouseButton == MouseButton::CustomKey && !validateCustomKey(ui, true)) {
        return;
    }
    if (ui->engine->isRunning()) {
        ui->engine->stop();
        return;
    }

    ui->engine->start(ui->profile.settings);
    if (delayManualStopButton && ui->engine->isRunning()) {
        ui->startStopButton->setEnabled(false);
        QTimer::singleShot(1000, this, [ui]() {
            if (ui->engine->isRunning()) {
                ui->startStopButton->setEnabled(true);
            }
        });
    }
}

void MainWindow::triggerHotkeyToggle()
{
    QVector<ProfileUi*> targets;
    for (ProfileUi* ui : profiles_) {
        if (ui->profile.enabled) {
            targets.append(ui);
        }
    }

    if (targets.isEmpty()) {
        return;
    }

    if (targets.first()->hotkeyDebounce.isValid() && targets.first()->hotkeyDebounce.elapsed() < 250) {
        return;
    }
    for (ProfileUi* ui : targets) {
        ui->hotkeyDebounce.restart();
    }

    const bool shouldStart = std::any_of(targets.begin(), targets.end(), [](const ProfileUi* ui) {
        return !ui->engine->isRunning();
    });

    for (ProfileUi* ui : targets) {
        if (shouldStart) {
            if (!ui->engine->isRunning()) {
                startOrStop(ui, false);
            }
        } else {
            ui->engine->stop();
        }
    }
}

void MainWindow::triggerHotkeyToggle(const QString& hotkeyText)
{
    QString error;
    const std::optional<Hotkey> parsed = parseHotkey(hotkeyText, &error);
    const QString normalized = parsed ? parsed->normalized : hotkeyText.trimmed();
    QVector<ProfileUi*> targets = profilesForHotkey(normalized);
    if (targets.isEmpty()) {
        return;
    }

    if (targets.first()->hotkeyDebounce.isValid() && targets.first()->hotkeyDebounce.elapsed() < 250) {
        return;
    }
    for (ProfileUi* ui : targets) {
        ui->hotkeyDebounce.restart();
    }

    const bool shouldStart = std::any_of(targets.begin(), targets.end(), [](const ProfileUi* ui) {
        return !ui->engine->isRunning();
    });

    for (ProfileUi* ui : targets) {
        if (shouldStart) {
            if (!ui->engine->isRunning()) {
                startOrStop(ui, false);
            }
        } else {
            ui->engine->stop();
        }
    }
}

void MainWindow::setRunningUi(ProfileUi* ui, bool running)
{
    if (running) {
        ui->statusLabel->setText(QStringLiteral("Running"));
        ui->startStopButton->setText(QStringLiteral("Stop"));
        ui->startStopButton->setEnabled(true);
    } else {
        ui->statusLabel->setText(QStringLiteral("Stopped"));
        ui->startStopButton->setText(QStringLiteral("Start clicking"));
        ui->startStopButton->setEnabled(true);
    }
    statusBar()->showMessage(running ? QStringLiteral("Running") : QStringLiteral("Stopped"), 3000);
    updateTrayState();
}

void MainWindow::setupTray()
{
    trayIcon_ = new QSystemTrayIcon(QIcon::fromTheme(QStringLiteral("io.github.omniclicker")), this);
    trayIcon_->setToolTip(QStringLiteral("Omni Clicker"));

    auto* menu = new QMenu(this);
    auto* showAction = menu->addAction(QStringLiteral("Show Omni Clicker"));
    auto* toggleAction = menu->addAction(QStringLiteral("Start clicking"));
    menu->addSeparator();
    auto* quitAction = menu->addAction(QStringLiteral("Quit"));

    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(toggleAction, &QAction::triggered, this, [this]() {
        triggerHotkeyToggle();
    });
    connect(quitAction, &QAction::triggered, this, [this]() {
        keepRunningInBackground_ = false;
        syncGlobalSettings(nullptr);
        for (ProfileUi* ui : profiles_) {
            ui->engine->stop();
        }
        hotkeys_.stop();
        qApp->quit();
    });
    connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            showNormal();
            raise();
            activateWindow();
        }
    });

    trayIcon_->setContextMenu(menu);
}

void MainWindow::updateTrayState()
{
    if (!trayIcon_) {
        return;
    }

    quint64 totalClicks = 0;
    int runningCount = 0;
    for (const ProfileUi* ui : profiles_) {
        totalClicks += ui->engine->clickCount();
        if (ui->engine->isRunning()) {
            ++runningCount;
        }
    }

    trayIcon_->setToolTip(QStringLiteral("Omni Clicker - %1 active - %2 clicks")
                              .arg(runningCount)
                              .arg(totalClicks));

    if (QMenu* menu = trayIcon_->contextMenu()) {
        const QList<QAction*> actions = menu->actions();
        if (actions.size() >= 2) {
            actions.at(1)->setText(runningCount > 0 ? QStringLiteral("Stop clicking") : QStringLiteral("Start clicking"));
        }
    }
}

void MainWindow::updateTabText(ProfileUi* ui)
{
    const int index = tabs_->indexOf(ui->page);
    if (index < 0) {
        return;
    }

    const ClickSettings settings = settingsFromUi(ui);
    const int addButtonWidth = profiles_.size() < MaximumProfiles ? addTabButton_->width() + 10 : 0;
    const int availableWidth = tabs_->tabBar()->width() - addButtonWidth;
    const int tabWidth = availableWidth / std::max(1, tabs_->count());
    QString text;
    if (tabWidth < 80) {
        text = settings.hotkey;
    } else if (tabWidth < 125) {
        text = QStringLiteral("%1 | %2").arg(settings.hotkey, tabIntervalSummary(settings));
    } else if (tabWidth < 175) {
        const QString button = settings.mouseButton == MouseButton::CustomKey
            ? QStringLiteral("key")
            : mouseButtonToString(settings.mouseButton).left(1).toUpper();
        text = QStringLiteral("%1 | %2 | %3").arg(settings.hotkey, button, tabIntervalSummary(settings));
    } else {
        text = tabSummary(settings);
    }
    if (!ui->profile.enabled) {
        text.prepend(QStringLiteral("Off | "));
    }
    tabs_->setTabText(index, text);
}

void MainWindow::updateAllTabTexts()
{
    for (ProfileUi* ui : profiles_) {
        updateTabText(ui);
    }
}

void MainWindow::updateTabCloseButtons()
{
    if (!tabs_) {
        return;
    }

    QTabBar* tabBar = tabs_->tabBar();
    const bool canClose = profiles_.size() > 1;
    for (int index = 0; index < tabs_->count(); ++index) {
        auto* closeButton = qobject_cast<QToolButton*>(tabBar->tabButton(index, QTabBar::RightSide));
        if (!closeButton) {
            closeButton = new QToolButton(tabBar);
            closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
            closeButton->setToolTip(QStringLiteral("Close tab"));
            closeButton->setAutoRaise(true);
            closeButton->setFixedSize(20, 20);
            closeButton->setStyleSheet(QStringLiteral(
                "QToolButton { border: 0; border-radius: 10px; padding: 0; }"
                "QToolButton:hover { background-color: palette(midlight); }"));
            tabBar->setTabButton(index, QTabBar::RightSide, closeButton);
            connect(closeButton, &QToolButton::clicked, this, [this, closeButton]() {
                const int tabIndex = tabs_->tabBar()->tabAt(closeButton->geometry().center());
                if (tabIndex >= 0) {
                    QWidget* page = tabs_->widget(tabIndex);
                    for (ProfileUi* ui : profiles_) {
                        if (ui->page == page) {
                            removeProfile(ui);
                            break;
                        }
                    }
                }
            });
        }

        closeButton->setVisible(canClose && (index == tabs_->currentIndex() || index == hoveredTabIndex_));
    }

    positionAddTabButton();
}

void MainWindow::updateTabSizing()
{
    if (!tabs_ || !addTabButton_ || tabs_->count() == 0) {
        return;
    }

    QTabBar* tabBar = tabs_->tabBar();
    if (tabBar->width() <= 0) {
        return;
    }

    const int addButtonWidth = profiles_.size() < MaximumProfiles ? addTabButton_->width() + 10 : 0;
    const int availableWidth = std::max(52, tabBar->width() - addButtonWidth);
    const int tabWidth = std::clamp(availableWidth / tabs_->count(), 52, 220);
    if (tabWidth != tabWidth_) {
        tabWidth_ = tabWidth;
        tabBar->setStyleSheet(QStringLiteral(
            "QTabBar::tab { width: %1px; }"
            "QTabBar::tab:hover { background-color: palette(midlight); }").arg(tabWidth));
    }
    updateAllTabTexts();
    QTimer::singleShot(0, this, &MainWindow::positionAddTabButton);
}

void MainWindow::positionAddTabButton()
{
    if (!tabs_ || !addTabButton_ || tabs_->count() == 0 || profiles_.size() >= MaximumProfiles) {
        return;
    }

    QTabBar* tabBar = tabs_->tabBar();
    QRect lastTab;
    for (int index = 0; index < tabs_->count(); ++index) {
        const QRect tabRect = tabBar->tabRect(index);
        if (tabRect.right() > lastTab.right()) {
            lastTab = tabRect;
        }
    }
    const int x = std::min(lastTab.right() + 5, tabBar->width() - addTabButton_->width());
    const int y = std::max(0, (tabBar->height() - addTabButton_->height()) / 2);
    addTabButton_->move(std::max(0, x), y);
}

void MainWindow::setFocusedShortcutsEnabled(bool enabled)
{
    for (QShortcut* shortcut : focusedShortcuts_) {
        shortcut->setEnabled(enabled);
    }
}

void MainWindow::syncGlobalSettings(ProfileUi* source)
{
    const bool wasLoading = loading_;
    loading_ = true;
    for (ProfileUi* ui : profiles_) {
        if (ui == source) {
            continue;
        }
        ui->startMinimizedCheck->setChecked(startMinimized_);
        ui->keepRunningInBackgroundCheck->setChecked(keepRunningInBackground_);
    }
    loading_ = wasLoading;
}

void MainWindow::createNewProfile()
{
    if (profiles_.size() >= MaximumProfiles) {
        return;
    }

    ClickProfile profile;
    profile.settings.startMinimized = startMinimized_;
    profile.settings.keepRunningInBackground = keepRunningInBackground_;
    ProfileUi* ui = addProfile(profile, true);
    refreshSummary(ui);
    refreshBackendCapabilities(ui);
    persistSettings();
    registerHotkeys();
}

void MainWindow::removeCurrentProfile()
{
    removeProfile(currentProfile());
}

void MainWindow::removeProfile(ProfileUi* ui)
{
    if (profiles_.size() <= 1 || !ui) {
        return;
    }

    ui->engine->stop();
    const int index = tabs_->indexOf(ui->page);
    if (index >= 0) {
        tabs_->removeTab(index);
    }
    profiles_.removeOne(ui);
    hoveredTabIndex_ = -1;
    ui->page->deleteLater();
    ui->engine->deleteLater();
    delete ui;
    persistSettings();
    registerHotkeys();
    updateTabCloseButtons();
    addTabButton_->setVisible(true);
    updateTabSizing();
}

bool MainWindow::validateCustomKey(ProfileUi* ui, bool interactive)
{
    if (!ui->buttonCombo || mouseButtonFromInt(comboValue(ui->buttonCombo)) != MouseButton::CustomKey) {
        return true;
    }

    QString error;
    const std::optional<Hotkey> custom = parseHotkey(ui->customKeyEdit->text(), &error);
    if (!custom) {
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Invalid custom key"), error);
        }
        statusBar()->showMessage(error, 5000);
        return false;
    }

    const std::optional<Hotkey> toggle = parseHotkey(ui->profile.settings.hotkey, nullptr);
    if (toggle && custom->normalized == toggle->normalized) {
        const QString message = QStringLiteral("The custom key cannot be the same as the activation hotkey. Choose a different key.");
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Key conflict"), message);
        }
        statusBar()->showMessage(message, 6000);
        return false;
    }

    if (ui->customKeyEdit->text() != custom->normalized) {
        ui->customKeyEdit->setText(custom->normalized);
    }
    return true;
}

MainWindow::ProfileUi* MainWindow::currentProfile() const
{
    const int index = tabs_ ? tabs_->currentIndex() : -1;
    if (index < 0 || index >= profiles_.size()) {
        return nullptr;
    }
    QWidget* page = tabs_->widget(index);
    for (ProfileUi* ui : profiles_) {
        if (ui->page == page) {
            return ui;
        }
    }
    return nullptr;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (tabs_ && watched == tabs_->tabBar()) {
        if (event->type() == QEvent::MouseMove) {
            const auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const int hoveredIndex = tabs_->tabBar()->tabAt(mouseEvent->pos());
            if (hoveredTabIndex_ != hoveredIndex) {
                hoveredTabIndex_ = hoveredIndex;
                updateTabCloseButtons();
            }
        } else if (event->type() == QEvent::Leave && hoveredTabIndex_ != -1) {
            hoveredTabIndex_ = -1;
            updateTabCloseButtons();
        } else if (event->type() == QEvent::Resize) {
            updateTabSizing();
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

QVector<MainWindow::ProfileUi*> MainWindow::profilesForHotkey(const QString& hotkeyText) const
{
    QVector<ProfileUi*> result;
    for (ProfileUi* ui : profiles_) {
        if (!ui->profile.enabled) {
            continue;
        }
        if (ui->profile.settings.hotkey == hotkeyText) {
            result.append(ui);
        }
    }
    return result;
}
