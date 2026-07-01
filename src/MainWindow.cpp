#include "MainWindow.h"

#include "Config.h"
#include "backends/InputBackend.h"
#include "hotkeys/HotkeyCaptureEdit.h"
#include "hotkeys/Hotkey.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QCoreApplication>
#include <QVBoxLayout>

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif
#ifndef BUILD_TYPE
#define BUILD_TYPE "unknown"
#endif
#ifndef GIT_HASH
#define GIT_HASH "unknown"
#endif
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "unknown"
#endif

#include <algorithm>

namespace {

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

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , settings_(AppConfig::load())
{
    buildUi();
    setupTray();
    connectSignals();
    applySettingsToUi();
    refreshBackendCapabilities();
    registerHotkey();
}

MainWindow::~MainWindow()
{
    hotkeys_.stop();
    engine_.stop();
}

bool MainWindow::shouldStartMinimized() const
{
    return settings_.startMinimized;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    persistSettings();
    if (settings_.keepRunningInBackground && trayIcon_ && trayIcon_->isVisible()) {
        hide();
        event->ignore();
        trayIcon_->showMessage(QStringLiteral("Omni Clicker"),
                               QStringLiteral("Omni Clicker is still running in the background."),
                               QSystemTrayIcon::Information,
                               2500);
        return;
    }

    engine_.stop();
    hotkeys_.stop();
    event->accept();
    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Omni Clicker"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("io.github.omniclicker")));
    resize(760, 520);
    setMinimumSize(680, 460);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(12);

    auto* header = new QHBoxLayout();
    auto* titleBlock = new QVBoxLayout();
    auto* title = new QLabel(QStringLiteral("Omni Clicker"), central);
    title->setObjectName(QStringLiteral("Title"));
    auto* subtitle = new QLabel(QStringLiteral("Native clicking controls for X11 and Wayland-aware sessions."), central);
    subtitle->setObjectName(QStringLiteral("Subtitle"));
    titleBlock->addWidget(title);
    titleBlock->addWidget(subtitle);

    startStopButton_ = new QPushButton(QStringLiteral("Start clicking"), central);
    startStopButton_->setObjectName(QStringLiteral("PrimaryButton"));
    startStopButton_->setMinimumHeight(42);
    startStopButton_->setMinimumWidth(170);

    header->addLayout(titleBlock, 1);
    header->addWidget(startStopButton_);
    root->addLayout(header);

    auto* content = new QHBoxLayout();
    content->setSpacing(12);

    auto* leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(12);
    leftColumn->addWidget(createClickOptionsGroup());
    leftColumn->addWidget(createSettingsGroup());
    leftColumn->addStretch();
    donateButton_ = new QPushButton(QStringLiteral("Donate"), central);
    leftColumn->addWidget(donateButton_, 0, Qt::AlignLeft);

    auto* rightColumn = new QVBoxLayout();
    rightColumn->setSpacing(12);
    rightColumn->addWidget(createStatusGroup());
    rightColumn->addWidget(createHotkeyGroup());
    rightColumn->addStretch();
    auto* applyRow = new QHBoxLayout();
    applyRow->addStretch();
    resetButton_ = new QPushButton(QStringLiteral("Reset"), central);
    applyRow->addWidget(resetButton_);
    applyRow->addWidget(applyHotkeyButton_);
    rightColumn->addLayout(applyRow);

    content->addLayout(leftColumn, 3);
    content->addLayout(rightColumn, 2);
    root->addLayout(content, 1);

    focusedShortcut_ = new QShortcut(this);
    focusedShortcut_->setContext(Qt::ApplicationShortcut);
    focusedShortcut_->setEnabled(false);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Stopped"));

    auto* buildInfoLabel = new QLabel(QStringLiteral("v" APP_VERSION " (" BUILD_TIMESTAMP ")"));
    buildInfoLabel->setStyleSheet(QStringLiteral("color: #888;"));
    statusBar()->addPermanentWidget(buildInfoLabel);
}

QGroupBox* MainWindow::createClickOptionsGroup()
{
    auto* group = new QGroupBox(QStringLiteral("Click options"), this);
    auto* layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);

    intervalSpin_ = new QSpinBox(group);
    intervalSpin_->setRange(1, 999999);
    intervalSpin_->setMinimumWidth(92);

    intervalUnitCombo_ = new QComboBox(group);
    addComboItem(intervalUnitCombo_, QStringLiteral("milliseconds"), static_cast<int>(IntervalUnit::Milliseconds));
    addComboItem(intervalUnitCombo_, QStringLiteral("seconds"), static_cast<int>(IntervalUnit::Seconds));
    addComboItem(intervalUnitCombo_, QStringLiteral("minutes"), static_cast<int>(IntervalUnit::Minutes));

    layout->addWidget(fieldLabel(QStringLiteral("Interval"), group), 0, 0);
    layout->addWidget(intervalSpin_, 0, 1);
    layout->addWidget(intervalUnitCombo_, 0, 2, 1, 2);

    buttonCombo_ = new QComboBox(group);
    addComboItem(buttonCombo_, QStringLiteral("Left click"), static_cast<int>(MouseButton::Left));
    addComboItem(buttonCombo_, QStringLiteral("Right click"), static_cast<int>(MouseButton::Right));
    addComboItem(buttonCombo_, QStringLiteral("Middle click"), static_cast<int>(MouseButton::Middle));
    addComboItem(buttonCombo_, QStringLiteral("Custom key"), static_cast<int>(MouseButton::CustomKey));

    customKeyEdit_ = new HotkeyCaptureEdit(group);
    customKeyEdit_->setPlaceholderText(QStringLiteral("Press the key to repeat"));
    customKeyEdit_->setToolTip(QStringLiteral("This key is pressed at each interval while Omni Clicker is running."));

    clickTypeCombo_ = new QComboBox(group);
    addComboItem(clickTypeCombo_, QStringLiteral("Single"), static_cast<int>(ClickType::Single));
    addComboItem(clickTypeCombo_, QStringLiteral("Double"), static_cast<int>(ClickType::Double));

    layout->addWidget(fieldLabel(QStringLiteral("Button"), group), 1, 0);
    layout->addWidget(buttonCombo_, 1, 1);
    layout->addWidget(fieldLabel(QStringLiteral("Type"), group), 1, 2);
    layout->addWidget(clickTypeCombo_, 1, 3);

    customKeyLabel_ = fieldLabel(QStringLiteral("Custom key"), group);
    layout->addWidget(customKeyLabel_, 2, 0);
    layout->addWidget(customKeyEdit_, 2, 1, 1, 3);

    infiniteCheck_ = new QCheckBox(QStringLiteral("Run until stopped"), group);
    limitSpin_ = new QSpinBox(group);
    limitSpin_->setRange(1, 1000000000);

    layout->addWidget(infiniteCheck_, 3, 0, 1, 2);
    layout->addWidget(fieldLabel(QStringLiteral("Limit"), group), 3, 2);
    layout->addWidget(limitSpin_, 3, 3);

    randomizeCheck_ = new QCheckBox(QStringLiteral("Randomize"), group);
    jitterSpin_ = new QSpinBox(group);
    jitterSpin_->setRange(1, 95);
    jitterSpin_->setSuffix(QStringLiteral("%"));

    layout->addWidget(randomizeCheck_, 4, 0, 1, 2);
    layout->addWidget(fieldLabel(QStringLiteral("Jitter"), group), 4, 2);
    layout->addWidget(jitterSpin_, 4, 3);

    return group;
}

QGroupBox* MainWindow::createHotkeyGroup()
{
    auto* group = new QGroupBox(QStringLiteral("Hotkey"), this);
    auto* layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(8);
    layout->setColumnStretch(1, 1);

    hotkeyEdit_ = new HotkeyCaptureEdit(group);
    hotkeyEdit_->setToolTip(QStringLiteral("Click here, press the shortcut, then click Apply."));
    applyHotkeyButton_ = new QPushButton(QStringLiteral("Apply"), this);
    layout->addWidget(fieldLabel(QStringLiteral("Toggle"), group), 0, 0);
    layout->addWidget(hotkeyEdit_, 0, 1);

    return group;
}

QGroupBox* MainWindow::createSettingsGroup()
{
    auto* group = new QGroupBox(QStringLiteral("Settings"), this);
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(8);

    startMinimizedCheck_ = new QCheckBox(QStringLiteral("Start minimized"), group);
    startMinimizedCheck_->setToolTip(QStringLiteral("Start as a minimized taskbar window instead of opening in the foreground."));

    keepRunningInBackgroundCheck_ = new QCheckBox(QStringLiteral("Keep running in background with tray icon"), group);
    keepRunningInBackgroundCheck_->setToolTip(QStringLiteral("When enabled, closing the window hides Omni Clicker to the tray instead of quitting."));

    layout->addWidget(startMinimizedCheck_);
    layout->addWidget(keepRunningInBackgroundCheck_);

    return group;
}

QGroupBox* MainWindow::createStatusGroup()
{
    auto* group = new QGroupBox(QStringLiteral("Status"), this);
    auto* layout = new QGridLayout(group);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);

    statusLabel_ = new QLabel(QStringLiteral("Stopped"), group);
    intervalSummaryLabel_ = new QLabel(group);
    buttonSummaryLabel_ = new QLabel(group);
    counterLabel_ = new QLabel(QStringLiteral("0"), group);
    backendLabel_ = new QLabel(QStringLiteral("Detecting..."), group);
    backendLabel_->setWordWrap(true);
    backendLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    limitationText_ = new QPlainTextEdit(group);
    limitationText_->setReadOnly(true);
    limitationText_->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    limitationText_->setMinimumHeight(90);
    limitationText_->setMaximumHeight(130);
    limitationText_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    layout->addWidget(fieldLabel(QStringLiteral("State:"), group), 0, 0);
    layout->addWidget(statusLabel_, 0, 1);
    layout->addWidget(fieldLabel(QStringLiteral("Clicks"), group), 0, 2);
    layout->addWidget(counterLabel_, 0, 3);
    layout->addWidget(fieldLabel(QStringLiteral("Interval:"), group), 1, 0);
    layout->addWidget(intervalSummaryLabel_, 1, 1);
    layout->addWidget(fieldLabel(QStringLiteral("Button"), group), 1, 2);
    layout->addWidget(buttonSummaryLabel_, 1, 3);
    layout->addWidget(fieldLabel(QStringLiteral("Backend:"), group), 2, 0);
    layout->addWidget(backendLabel_, 2, 1, 1, 3);
    layout->addWidget(fieldLabel(QStringLiteral("Notes:"), group), 3, 0, Qt::AlignTop);
    layout->addWidget(limitationText_, 3, 1, 1, 3);

    return group;
}


void MainWindow::connectSignals()
{
    const auto persist = [this]() {
        persistSettings();
    };

    connect(intervalSpin_, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(intervalUnitCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, persist);
    connect(buttonCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, persist);
    connect(buttonCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this]() {
        refreshSummary();
    });
    connect(customKeyEdit_, &HotkeyCaptureEdit::hotkeyCaptured, this, [this](const QString&) {
        validateCustomKey(true);
        persistSettings();
    });
    connect(customKeyEdit_, &HotkeyCaptureEdit::recordingChanged, this, [this](bool recording) {
        if (recording) {
            hotkeys_.stop();
            return;
        }
        registerHotkey();
    });
    connect(clickTypeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, persist);
    connect(infiniteCheck_, &QCheckBox::toggled, this, persist);
    connect(limitSpin_, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(randomizeCheck_, &QCheckBox::toggled, this, persist);
    connect(jitterSpin_, qOverload<int>(&QSpinBox::valueChanged), this, persist);
    connect(startMinimizedCheck_, &QCheckBox::toggled, this, persist);
    connect(keepRunningInBackgroundCheck_, &QCheckBox::toggled, this, [this](bool enabled) {
        persistSettings();
        if (trayIcon_) {
            trayIcon_->setVisible(enabled);
        }
    });
    connect(hotkeyEdit_, &HotkeyCaptureEdit::recordingChanged, this, [this](bool recording) {
        focusedShortcut_->setEnabled(!recording && !focusedShortcut_->key().isEmpty());
        if (recording) {
            hotkeys_.stop();
            return;
        }

        QTimer::singleShot(0, this, [this]() {
            if (QApplication::focusWidget() != applyHotkeyButton_) {
                registerHotkey();
            }
        });
    });
    connect(hotkeyEdit_, &HotkeyCaptureEdit::hotkeyCaptured, this, [this](const QString& hotkeyText) {
        statusBar()->showMessage(QStringLiteral("Pending hotkey: %1. Click Apply to save it.").arg(hotkeyText), 4000);
    });
    connect(applyHotkeyButton_, &QPushButton::clicked, this, &MainWindow::applyHotkeyFromUi);
    connect(resetButton_, &QPushButton::clicked, this, &MainWindow::resetToDefaults);
    connect(donateButton_, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://ko-fi.com/limonyx")));
    });

    connect(focusedShortcut_, &QShortcut::activated, this, &MainWindow::triggerHotkeyToggle);
    connect(focusedShortcut_, &QShortcut::activatedAmbiguously, this, &MainWindow::triggerHotkeyToggle);
    connect(startStopButton_, &QPushButton::clicked, this, [this]() {
        startOrStop(true);
    });

    connect(&engine_, &ClickEngine::runningChanged, this, &MainWindow::setRunningUi);
    connect(&engine_, &ClickEngine::clickCountChanged, this, [this](quint64 count) {
        counterLabel_->setText(QString::number(count));
        updateTrayState();
    });
    connect(&engine_, &ClickEngine::backendChanged, this, [this](const QString& name, const QString& details) {
        backendLabel_->setText(name);
        limitationText_->setPlainText(details.isEmpty() ? QStringLiteral("Full feature set available.") : details);
    });
    connect(&engine_, &ClickEngine::errorOccurred, this, [this](const QString& message) {
        statusBar()->showMessage(message, 8000);
        QMessageBox::warning(this, QStringLiteral("Autoclicker error"), message);
    });

    connect(&hotkeys_, &GlobalHotkeyManager::activated, this, &MainWindow::triggerHotkeyToggle);
    connect(&hotkeys_, &GlobalHotkeyManager::errorOccurred, this, [this](const QString& message) {
        statusBar()->showMessage(message, 8000);
        limitationText_->setPlainText(message);
    });
    connect(&hotkeys_, &GlobalHotkeyManager::statusChanged, this, [this](const QString& message) {
        statusBar()->showMessage(message, 5000);
        limitationText_->setPlainText(message);
    });
}

void MainWindow::applySettingsToUi()
{
    loading_ = true;

    intervalSpin_->setValue(settings_.intervalValue);
    setComboValue(intervalUnitCombo_, static_cast<int>(settings_.intervalUnit));
    setComboValue(buttonCombo_, static_cast<int>(settings_.mouseButton));
    setComboValue(clickTypeCombo_, static_cast<int>(settings_.clickType));
    customKeyEdit_->setText(settings_.customKey);
    infiniteCheck_->setChecked(settings_.infinite);
    limitSpin_->setValue(static_cast<int>(std::min<quint64>(settings_.clickLimit, 1000000000ULL)));
    randomizeCheck_->setChecked(settings_.randomizeInterval);
    jitterSpin_->setValue(settings_.randomJitterPercent);
    hotkeyEdit_->setText(settings_.hotkey);
    startMinimizedCheck_->setChecked(settings_.startMinimized);
    keepRunningInBackgroundCheck_->setChecked(settings_.keepRunningInBackground);
    if (trayIcon_) {
        trayIcon_->setVisible(settings_.keepRunningInBackground);
    }

    loading_ = false;
    refreshSummary();
}

ClickSettings MainWindow::settingsFromUi() const
{
    ClickSettings settings;
    settings.intervalValue = intervalSpin_->value();
    settings.intervalUnit = intervalUnitFromInt(comboValue(intervalUnitCombo_));
    settings.mouseButton = mouseButtonFromInt(comboValue(buttonCombo_));
    settings.clickType = clickTypeFromInt(comboValue(clickTypeCombo_));
    settings.positionMode = PositionMode::CurrentCursor;
    settings.fixedPosition = QPoint(0, 0);
    settings.customKey = customKeyEdit_->text().trimmed();
    settings.infinite = infiniteCheck_->isChecked();
    settings.clickLimit = static_cast<quint64>(limitSpin_->value());
    settings.randomizeInterval = randomizeCheck_->isChecked();
    settings.randomJitterPercent = jitterSpin_->value();
    settings.hotkey = settings_.hotkey;
    settings.startMinimized = startMinimizedCheck_->isChecked();
    settings.keepRunningInBackground = keepRunningInBackgroundCheck_->isChecked();
    return settings;
}

void MainWindow::persistSettings()
{
    if (loading_) {
        return;
    }

    settings_ = settingsFromUi();
    if (settings_.mouseButton == MouseButton::CustomKey && !validateCustomKey(false)) {
        return;
    }
    limitSpin_->setEnabled(!settings_.infinite);
    jitterSpin_->setEnabled(settings_.randomizeInterval);
    refreshSummary();

    QString error;
    if (!AppConfig::save(settings_, &error)) {
        statusBar()->showMessage(error, 8000);
    }
}

void MainWindow::refreshSummary()
{
    const ClickSettings current = settingsFromUi();
    intervalSummaryLabel_->setText(intervalSummary(current));
    const bool customKey = current.mouseButton == MouseButton::CustomKey;
    customKeyLabel_->setVisible(customKey);
    customKeyEdit_->setEnabled(customKey);
    customKeyEdit_->setVisible(customKey);
    buttonSummaryLabel_->setText(customKey
                                     ? QStringLiteral("key %1").arg(current.customKey)
                                     : QStringLiteral("%1, %2").arg(mouseButtonToString(current.mouseButton), clickTypeToString(current.clickType)));
    limitSpin_->setEnabled(!current.infinite);
    jitterSpin_->setEnabled(current.randomizeInterval);
    updateTrayState();
}

void MainWindow::refreshBackendCapabilities()
{
    QString error;
    std::unique_ptr<InputBackend> backend = createInputBackend(&error);
    if (!backend) {
        backendLabel_->setText(QStringLiteral("Unavailable"));
        limitationText_->setPlainText(error);
        return;
    }

    if (!backend->initialize(&error)) {
        backendLabel_->setText(backend->name());
        limitationText_->setPlainText(error);
        return;
    }

    const BackendCapabilities caps = backend->capabilities();
    backendLabel_->setText(backend->name());
    limitationText_->setPlainText(caps.limitations.isEmpty() ? QStringLiteral("Full feature set available.") : caps.limitations.join(QStringLiteral("\n")));
}

void MainWindow::registerHotkey()
{
    QString error;
    const auto parsed = parseHotkey(settings_.hotkey, &error);
    if (!parsed) {
        focusedShortcut_->setEnabled(false);
        hotkeys_.stop();
        limitationText_->setPlainText(error);
        return;
    }

    settings_.hotkey = parsed->normalized;
    hotkeyEdit_->setText(settings_.hotkey);

    focusedShortcut_->setKey(hotkeyToKeySequence(*parsed));
    focusedShortcut_->setEnabled(true);
    hotkeys_.setHotkeyText(settings_.hotkey);
}

void MainWindow::applyHotkeyFromUi()
{
    QString error;
    const std::optional<Hotkey> parsed = parseHotkey(hotkeyEdit_->text(), &error);
    if (!parsed) {
        statusBar()->showMessage(error, 5000);
        QMessageBox::warning(this, QStringLiteral("Invalid hotkey"), error);
        return;
    }

    settings_.hotkey = parsed->normalized;
    if (!validateCustomKey(true)) {
        return;
    }
    hotkeyEdit_->setText(settings_.hotkey);
    persistSettings();
    registerHotkey();
    hotkeyEdit_->clearFocus();
    setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(QStringLiteral("Hotkey applied: %1").arg(settings_.hotkey), 4000);
}

void MainWindow::resetToDefaults()
{
    if (engine_.isRunning()) {
        engine_.stop();
    }

    settings_ = ClickSettings {};
    applySettingsToUi();
    persistSettings();
    registerHotkey();
    hotkeyEdit_->clearFocus();
    setFocus(Qt::OtherFocusReason);
    statusBar()->showMessage(QStringLiteral("Settings reset to defaults."), 4000);
}

void MainWindow::startOrStop(bool delayManualStopButton)
{
    persistSettings();
    if (settings_.mouseButton == MouseButton::CustomKey && !validateCustomKey(true)) {
        return;
    }
    if (engine_.isRunning()) {
        engine_.stop();
        return;
    }

    engine_.start(settings_);
    if (delayManualStopButton && engine_.isRunning()) {
        startStopButton_->setEnabled(false);
        QTimer::singleShot(1000, this, [this]() {
            if (engine_.isRunning()) {
                startStopButton_->setEnabled(true);
            }
        });
    }
}

void MainWindow::triggerHotkeyToggle()
{
    if (hotkeyDebounce_.isValid() && hotkeyDebounce_.elapsed() < 250) {
        return;
    }

    hotkeyDebounce_.restart();
    startOrStop(false);
}

void MainWindow::setRunningUi(bool running)
{
    if (running) {
        statusLabel_->setText(QStringLiteral("Running"));
        startStopButton_->setText(QStringLiteral("Stop"));
        startStopButton_->setEnabled(true);
    } else {
        statusLabel_->setText(QStringLiteral("Stopped"));
        startStopButton_->setText(QStringLiteral("Start clicking"));
        startStopButton_->setEnabled(true);
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
        startOrStop(false);
    });
    connect(quitAction, &QAction::triggered, this, [this]() {
        settings_.keepRunningInBackground = false;
        if (keepRunningInBackgroundCheck_) {
            keepRunningInBackgroundCheck_->setChecked(false);
        }
        engine_.stop();
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

    trayIcon_->setToolTip(QStringLiteral("Omni Clicker - %1 - %2 clicks")
                              .arg(engine_.isRunning() ? QStringLiteral("Running") : QStringLiteral("Stopped"))
                              .arg(engine_.clickCount()));

    if (QMenu* menu = trayIcon_->contextMenu()) {
        const QList<QAction*> actions = menu->actions();
        if (actions.size() >= 2) {
            actions.at(1)->setText(engine_.isRunning() ? QStringLiteral("Stop clicking") : QStringLiteral("Start clicking"));
        }
    }
}

bool MainWindow::validateCustomKey(bool interactive)
{
    if (!buttonCombo_ || mouseButtonFromInt(comboValue(buttonCombo_)) != MouseButton::CustomKey) {
        return true;
    }

    QString error;
    const std::optional<Hotkey> custom = parseHotkey(customKeyEdit_->text(), &error);
    if (!custom) {
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Invalid custom key"), error);
        }
        statusBar()->showMessage(error, 5000);
        return false;
    }

    const std::optional<Hotkey> toggle = parseHotkey(settings_.hotkey, nullptr);
    if (toggle && custom->normalized == toggle->normalized) {
        const QString message = QStringLiteral("The custom key cannot be the same as the activation hotkey. Choose a different key.");
        if (interactive) {
            QMessageBox::warning(this, QStringLiteral("Key conflict"), message);
        }
        statusBar()->showMessage(message, 6000);
        return false;
    }

    if (customKeyEdit_->text() != custom->normalized) {
        customKeyEdit_->setText(custom->normalized);
    }
    return true;
}
