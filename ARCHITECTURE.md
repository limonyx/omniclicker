# Omni Clicker Architecture

This document explains how Omni Clicker works internally, following the runtime flow through the actual code instead of summarizing files in isolation.

## 1. Project Overview

Omni Clicker is a native Linux autoclicker. It presents a Qt6 GUI where the user chooses an interval, mouse button or custom key, click type, optional click limit, optional interval randomization, a toggle hotkey, and background/tray behavior. When started, it repeatedly injects mouse or keyboard input through an input backend selected for the current display environment.

The project is a C++17 Qt Widgets application built by `CMakeLists.txt`. The main libraries are:

- Qt6 Widgets: GUI, tray icon, actions, signals/slots, dialogs, shortcuts.
- Qt6 DBus: xdg-desktop-portal GlobalShortcuts support.
- Qt6 Network: linked in CMake but not used directly by the current source code.
- X11 and libXtst: X11 display access, XTest input injection, X11 passive key grabs.
- Linux input/uinput headers: generic Wayland-compatible virtual input device.
- Optional KF6GlobalAccel: KDE Plasma global shortcut integration when found at build time.

The central runtime objects are:

- `MainWindow` in `src/MainWindow.cpp`: owns the GUI, the persisted `ClickSettings`, the `ClickEngine`, and the `GlobalHotkeyManager`.
- `ClickEngine` in `src/ClickEngine.cpp`: owns the worker thread that emits repeated clicks.
- `InputBackend` implementations in `src/backends/`: perform the actual click/key injection.
- `GlobalHotkeyManager` and `HotkeyBackend` implementations in `src/hotkeys/`: register the toggle shortcut through X11, KDE, portals, compositor IPC, GNOME settings, or evdev.
- `AppConfig` in `src/Config.cpp`: persists settings to an INI file with `QSettings`.

The major interaction pattern is:

1. `main()` creates `QApplication`, enforces single-instance behavior, constructs `MainWindow`, and enters the Qt event loop.
2. `MainWindow` loads settings, builds widgets, registers a hotkey, and reacts to UI changes by saving settings.
3. Starting the clicker calls `ClickEngine::start(settings)`.
4. `ClickEngine` creates an `InputBackend` on its worker thread and loops until stopped, reaching a limit, or receiving an injection error.
5. Global hotkeys call `MainWindow::triggerHotkeyToggle()`, which toggles the engine.

## 2. Application Startup Flow

### Entry Point

Execution begins in `src/main.cpp` at `main(int argc, char* argv[])`.

The first operation is `XInitThreads()`. This matters because the app can use Xlib from more than one thread or subsystem: the GUI is Qt, X11 input injection can happen in the click worker thread, and X11 global hotkeys run their own event thread.

Then `main()` sets Qt application metadata:

- `QApplication::setApplicationName("Omni Clicker")`
- `QApplication::setApplicationDisplayName("Omni Clicker")`
- `QApplication::setOrganizationName("omniclicker")`
- `QApplication::setDesktopFileName("io.github.omniclicker")`

The application object is created with `QApplication app(argc, argv)`, and `QApplication::setQuitOnLastWindowClosed(false)` keeps the process alive when the window is hidden to the tray.

### Command-Line Options

`QCommandLineParser` defines:

- `--start-minimized`: minimize the window at startup.
- `--toggle`: if an instance is already running, ask it to toggle clicking.

The `--toggle` option is important for GNOME, Sway, and Hyprland hotkey integrations. Those backends register a desktop/compositor shortcut that executes `omniclicker --toggle`; the running instance receives the request over a local socket.

### Single-Instance Flow

`main()` uses `QLocalSocket` and `QLocalServer` with the name `omniclicker-single-instance`.

If a socket connection succeeds, another instance is running:

- With `--toggle`, the new process writes `TOGGLE`.
- Otherwise, it writes `WAKEUP`.
- The new process exits.

If no existing server is reachable, the current process removes stale server state with `QLocalServer::removeServer(serverName)`, calls `server.listen(serverName)`, and becomes the primary instance.

The primary instance connects `QLocalServer::newConnection` to a handler that reads client messages:

- `WAKEUP`: calls `window.showNormal()`, `raise()`, and `activateWindow()`.
- `TOGGLE`: calls `window.triggerHotkeyToggle()`.

This is the bridge used by compositor shortcuts that launch a command rather than invoke an in-process callback.

### Main Window Construction

`main()` constructs `MainWindow window;`. The constructor in `src/MainWindow.cpp` initializes `settings_` from `AppConfig::load()` and then calls:

1. `buildUi()`
2. `setupTray()`
3. `connectSignals()`
4. `applySettingsToUi()`
5. `refreshBackendCapabilities()`
6. `registerHotkey()`

`window.show()` is called unconditionally, then `--start-minimized` or persisted `settings_.startMinimized` causes a zero-delay `QTimer::singleShot()` to minimize it after the event loop starts.

Finally, if the first instance was itself launched with `--toggle`, `main()` calls `window.triggerHotkeyToggle()` after showing/minimizing.

### Configuration Loading

`AppConfig::load()` in `src/Config.cpp` reads an INI file through `QSettings(AppConfig::configPath(), QSettings::IniFormat)`.

The path is:

- `QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/config.ini"`
- fallback: `~/.config/omniclicker/config.ini`

`ClickSettings` defaults live in `src/Types.h`. `AppConfig::load()` reads these groups:

- `[clicking]`: interval, unit, mouse button, click type, position mode, fixed X/Y, custom key, infinite flag, limit, randomization, jitter.
- `[hotkeys]`: toggle hotkey.
- `[ui]`: start minimized and keep running in background.

Saving happens through `AppConfig::save()`, which creates the config directory if needed, writes the same groups, calls `settings.sync()`, and returns an error if `QSettings::status()` is not `NoError`.

### Backend Selection

Click injection backend selection is in `src/backends/BackendFactory.cpp`:

- `OMNICLICKER_BACKEND=x11` forces `X11Backend`.
- `OMNICLICKER_BACKEND=uinput` or `OMNICLICKER_BACKEND=wayland` forces `UInputBackend`.
- Otherwise `detectedSessionType()` checks `XDG_SESSION_TYPE`, then `WAYLAND_DISPLAY`, then `DISPLAY`.
- Wayland selects `UInputBackend`.
- X11 or a present `DISPLAY` selects `X11Backend`.
- If nothing matches, backend creation fails with an explanatory error.

Global hotkey backend selection is separate and is implemented in `src/hotkeys/HotkeyBackendFactory.cpp`. It can be forced with `OMNICLICKER_HOTKEY_BACKEND`.

## 3. GUI Architecture

### Qt6 Structure

The GUI is a hand-built Qt Widgets interface in `MainWindow::buildUi()`. There are no `.ui` files. `CMakeLists.txt` enables `CMAKE_AUTOMOC`, `CMAKE_AUTORCC`, and `CMAKE_AUTOUIC`, but the current GUI code is written directly in C++.

`MainWindow` inherits `QMainWindow` and has `Q_OBJECT` because it uses Qt signals/slots and overrides `closeEvent()`.

The central widget has:

- A header with title/subtitle and the primary start/stop button.
- A left column containing click options, settings, and the donate button.
- A right column containing status, hotkey controls, reset, and apply.
- A `QStatusBar` with transient messages and build version info.

### Main Widget Groups

`MainWindow::createClickOptionsGroup()` builds:

- `intervalSpin_`: interval numeric value.
- `intervalUnitCombo_`: milliseconds, seconds, minutes.
- `buttonCombo_`: left, right, middle, custom key.
- `customKeyEdit_`: a `HotkeyCaptureEdit` used for repeated custom key injection.
- `clickTypeCombo_`: single or double.
- `infiniteCheck_` and `limitSpin_`.
- `randomizeCheck_` and `jitterSpin_`.

`MainWindow::createSettingsGroup()` builds:

- `startMinimizedCheck_`
- `keepRunningInBackgroundCheck_`

`MainWindow::createStatusGroup()` builds:

- `statusLabel_`
- interval and button summary labels.
- `counterLabel_`
- `backendLabel_`
- `limitationText_`

`MainWindow::createHotkeyGroup()` builds:

- `hotkeyEdit_`: a `HotkeyCaptureEdit` for the global toggle shortcut.
- `applyHotkeyButton_`: created here, but inserted later in the right-column apply row.

### User Actions and Signals

`MainWindow::connectSignals()` wires the GUI to application state.

Most value changes call the local `persist` lambda, which calls `persistSettings()`. That function:

1. Ignores changes while `loading_` is true.
2. Builds a fresh `ClickSettings` from the widgets via `settingsFromUi()`.
3. Validates custom key conflicts when needed.
4. Enables/disables the limit and jitter controls.
5. Refreshes the visible summary.
6. Saves with `AppConfig::save()`.

Start/stop behavior:

- The main button calls `startOrStop(true)`.
- The focused fallback shortcut and global hotkey both call `triggerHotkeyToggle()`.
- `triggerHotkeyToggle()` debounces activations within 250 ms using `QElapsedTimer hotkeyDebounce_`.
- `startOrStop()` persists settings, validates the custom key, stops if already running, or calls `engine_.start(settings_)`.

The GUI updates from engine signals:

- `ClickEngine::runningChanged` -> `MainWindow::setRunningUi()`.
- `ClickEngine::clickCountChanged` -> update counter label and tray tooltip.
- `ClickEngine::backendChanged` -> update backend label and limitations text.
- `ClickEngine::errorOccurred` -> status bar and warning dialog.

Hotkey manager signals are also displayed:

- `GlobalHotkeyManager::activated` -> `triggerHotkeyToggle()`.
- `errorOccurred` and `statusChanged` -> status bar and limitations text.

### Hotkey Capture Edit

`HotkeyCaptureEdit` in `src/hotkeys/HotkeyCaptureEdit.cpp` is a `QLineEdit` specialized for shortcuts:

- On focus in, it emits `recordingChanged(true)` and selects all text.
- On focus out, it emits `recordingChanged(false)`.
- On key press, Escape clears focus. Other keys are normalized through `hotkeyFromKeyEvent()` in `src/hotkeys/Hotkey.cpp`, then emitted as `hotkeyCaptured(normalized)`.

While the main hotkey field is recording, `MainWindow` disables the focused `QShortcut` and stops global hotkeys to avoid self-triggering. When recording ends, it re-registers the hotkey unless focus moved to the Apply button.

### Settings Storage and Updates

`MainWindow::applySettingsToUi()` copies `settings_` into widgets while `loading_` is true. This prevents the widget setters from immediately saving intermediate state.

`settingsFromUi()` creates a new `ClickSettings` from the widgets. One important limitation is visible here: it always sets `settings.positionMode = PositionMode::CurrentCursor` and `settings.fixedPosition = QPoint(0, 0)`. Although `ClickSettings`, `Config`, and `X11Backend` contain fixed-position support, the current GUI does not expose controls for fixed-position clicking.

`applyHotkeyFromUi()` is intentionally separate from ordinary persistence. Editing the hotkey field only stages a value. Clicking Apply parses and normalizes it, validates it against the custom repeated key, saves it, and re-registers global hotkeys.

## 4. Click Generation System

### Engine Lifecycle

The click engine is `ClickEngine` in `src/ClickEngine.cpp`.

`ClickEngine::start(ClickSettings settings)`:

1. Uses `running_.exchange(true)` to ignore duplicate starts.
2. Joins any previous worker thread.
3. Resets `clickCount_` to zero.
4. Emits `clickCountChanged(0)` and `runningChanged(true)`.
5. Starts `worker_ = std::thread(&ClickEngine::run, this, settings)`.

`ClickEngine::stop()`:

1. Sets `running_` to false.
2. Calls `wake_.notify_all()` to interrupt sleep.
3. Joins the worker thread unless called from the worker itself.
4. Emits `runningChanged(false)`.

`ClickEngine::~ClickEngine()` calls `stop()` and joins any remaining thread.

### Click Loop

`ClickEngine::run()` creates a fresh input backend with `createInputBackend()`, initializes it, emits its name/limitations, and then loops while `running_` is true.

Each loop iteration:

1. Checks the finite click limit: if `!settings.infinite && clickCount_ >= settings.clickLimit`, stop the loop.
2. Calls `backend->click(settings, &clickError)`.
3. Increments `clickCount_` and emits `clickCountChanged(count)`.
4. Computes the next interval with `nextIntervalMs(settings)`.
5. Advances an absolute `nextDeadline`.
6. If the backend took too long and the deadline is already missed, skips missed periods instead of firing catch-up bursts.
7. Calls `waitUntilDeadline(nextDeadline, interval)`.

The loop uses absolute deadlines so small scheduling delays do not accumulate over time.

### Timing and Randomization

`ClickSettings::intervalMilliseconds()` in `src/Types.cpp` converts the configured interval:

- milliseconds: `value`
- seconds: `value * 1000`
- minutes: `value * 60 * 1000`

`ClickEngine::nextIntervalMs()` clamps the base interval to at least 1 ms. If randomization is off, that base is used directly.

If randomization is enabled:

- `randomJitterPercent` is clamped to 0..95.
- `jitter = max(1, base * jitterPercent / 100)`.
- low bound is `max(1, base - jitter)`.
- high bound is `base + jitter`.
- `QRandomGenerator::global()->bounded(low, high + 1)` picks the interval.

`waitUntilDeadline()` sleeps on a `std::condition_variable` until just before the deadline, then yields in a short bounded spin window. The spin window is `min(100 microseconds, interval / 10)`. This is meant to reduce final scheduler jitter without permanently busy-waiting.

### Mouse Clicks

The common semantic choices are represented by `MouseButton` and `ClickType` in `src/Types.h`:

- `MouseButton::Left`
- `MouseButton::Right`
- `MouseButton::Middle`
- `MouseButton::CustomKey`
- `ClickType::Single`
- `ClickType::Double`

For mouse buttons, both input backends emit a press followed by a release. For double click, they repeat that sequence twice with a 35 ms sleep between the first and second click.

The README says "scroll wheel mouse click support". The actual code implements `MouseButton::Middle`, which is the middle mouse button or wheel press. There is no implementation for scroll-wheel up/down events such as X11 buttons 4/5 or uinput `REL_WHEEL`.

### X11 Clicks

`X11Backend::click()` in `src/backends/X11Backend.cpp` uses XTest:

- Left -> X11 button 1.
- Middle -> X11 button 2.
- Right -> X11 button 3.
- Press: `XTestFakeButtonEvent(display_, button, True, CurrentTime)`.
- Release: `XTestFakeButtonEvent(display_, button, False, CurrentTime)`.
- Flush: `XFlush(display_)`.

If `settings.positionMode == PositionMode::FixedCoordinate`, X11 first calls `XWarpPointer()` to move the pointer to `settings.fixedPosition`. As noted above, the current GUI always chooses `CurrentCursor`, so this path exists but is not exposed to users by the current interface.

### Wayland/uinput Clicks

`UInputBackend::click()` in `src/backends/UInputBackend.cpp` uses a virtual Linux input device:

- Left -> `BTN_LEFT`
- Middle -> `BTN_MIDDLE`
- Right -> `BTN_RIGHT`
- Press: write `input_event { EV_KEY, button, 1 }`.
- Release: write `input_event { EV_KEY, button, 0 }`.
- Sync after each event with `EV_SYN / SYN_REPORT`.

The backend refuses fixed-coordinate clicking with the error "Fixed-coordinate clicking is not available through the generic Wayland/uinput backend." It also cannot report current pointer position.

### Custom Keys

Custom key input reuses the hotkey parser:

- Text is parsed by `parseHotkey()` in `src/hotkeys/Hotkey.cpp`.
- The parser normalizes modifiers and keys into strings like `Ctrl + F6` or `Space`.
- `MainWindow::validateCustomKey()` rejects invalid keys and rejects a custom repeated key that equals the activation hotkey.

On X11:

- `X11Backend::click()` maps the parsed Qt key to an X11 `KeySym` with `qtKeyToKeySym()`.
- Modifiers map to left-side X11 keysyms such as `XK_Control_L`, `XK_Alt_L`, `XK_Shift_L`, and `XK_Super_L`.
- It presses modifiers, presses/releases the target key with `XTestFakeKeyEvent()`, then releases modifiers in reverse order.

On uinput:

- `UInputBackend::click()` maps the parsed Qt key to Linux input key codes such as `KEY_A`, `KEY_F1`, `KEY_SPACE`.
- Modifiers map to `KEY_LEFTCTRL`, `KEY_LEFTALT`, `KEY_LEFTSHIFT`, and `KEY_LEFTMETA`.
- It writes EV_KEY press/sync events for modifiers, target press/release, then modifier releases in reverse order.

Both backends double-repeat custom keys when `ClickType::Double` is selected.

## 5. Platform-Specific Implementations

### X11

X11 input simulation is implemented by `X11Backend`.

Initialization:

1. `XOpenDisplay(nullptr)` opens the display named by `DISPLAY`.
2. `XTestQueryExtension()` verifies the XTest extension is available.
3. `DefaultRootWindow(display_)` stores the root window for pointer warp and pointer queries.

Mouse injection uses `XTestFakeButtonEvent()`. Keyboard injection uses `XTestFakeKeyEvent()` after converting Qt keys to `KeySym` and then to a `KeyCode` with `XKeysymToKeycode()`.

Pointer support:

- Fixed coordinate clicking is supported internally through `XWarpPointer()`.
- Cursor capture is supported internally through `XQueryPointer()`.
- The GUI does not currently expose either feature.

X11 is selected automatically when `XDG_SESSION_TYPE=x11` or when no session type is set but `DISPLAY` exists. It can also be forced with `OMNICLICKER_BACKEND=x11`.

### Wayland Generic Input

Wayland input simulation is implemented by `UInputBackend`.

This code does not use a Wayland protocol to inject clicks. That is intentional: normal Wayland clients are not allowed to synthesize arbitrary global pointer input. Instead, Omni Clicker creates a kernel virtual input device through `/dev/uinput`.

Initialization:

1. Open `/dev/uinput` with `O_WRONLY | O_NONBLOCK | O_CLOEXEC`.
2. Enable event types `EV_KEY` and `EV_REL`.
3. Enable relative axes `REL_X` and `REL_Y`.
4. Enable all key bits from 1 to `KEY_MAX`.
5. Configure `uinput_setup` with name `Omni Clicker Virtual Input`.
6. Call `UI_DEV_SETUP` and `UI_DEV_CREATE`.
7. Sleep 100 ms to let the compositor discover the virtual device.

Although `EV_REL`, `REL_X`, and `REL_Y` are enabled, the backend currently does not move the pointer. It only sends button and keyboard key events.

Wayland limitations are reported by `UInputBackend::capabilities()`:

- Current-cursor clicking is supported.
- Fixed-position clicking is not supported.
- Cursor capture is not supported.
- Users are told that Wayland generally blocks global mouse reading/moving.

Wayland is selected automatically when `XDG_SESSION_TYPE=wayland` or `WAYLAND_DISPLAY` exists. It can also be forced with `OMNICLICKER_BACKEND=uinput` or `OMNICLICKER_BACKEND=wayland`.

### KDE Plasma Wayland

For click generation, KDE Plasma Wayland uses the same generic `UInputBackend` as other Wayland sessions.

For global hotkeys, `HotkeyBackendFactory` prefers `KdeGlobalAccelBackend` when:

- The session is Wayland.
- `XDG_CURRENT_DESKTOP` contains `KDE`.
- The project was built with KF6GlobalAccel, which defines `HAVE_KGLOBALACCEL`.

`KdeGlobalAccelBackend` creates a `QAction`, assigns object name `toggle-autoclicking`, connects `QAction::triggered` to the callback, and registers the shortcut with:

- `KGlobalAccel::self()->setDefaultShortcut()`
- `KGlobalAccel::self()->setShortcut()`
- `KGlobalAccel::self()->hasShortcut()`

If KGlobalAccel is not compiled in or fails, the factory then tries the portal backend, then X11 fallback, then evdev.

### GNOME Wayland

For click generation, GNOME Wayland uses `UInputBackend`.

For global hotkeys, `HotkeyBackendFactory` prefers `GnomeHotkeyBackend` when the session is Wayland and `XDG_CURRENT_DESKTOP` contains `GNOME`.

`GnomeHotkeyBackend` does not keep an in-process signal subscription. Instead, it writes a GNOME custom keyboard shortcut through `gsettings`:

- Schema base: `org.gnome.settings-daemon.plugins.media-keys`
- Custom path: `/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/omniclicker/`
- Name: `Omni Clicker Toggle`
- Binding: converted to GTK accelerator syntax, for example `Ctrl+F6` becomes `<Primary>F6`.
- Command:
  - Flatpak: `flatpak run io.github.omniclicker --toggle`
  - Non-Flatpak: current executable path plus `--toggle`

When GNOME invokes the shortcut command, the new process connects to the existing primary instance and sends `TOGGLE` through the local socket.

Inside Flatpak, `GnomeHotkeyBackend` runs `flatpak-spawn --host gsettings ...`.

The backend removes its custom keybinding in `stop()` by editing the custom keybinding list and calling `gsettings reset-recursively`.

### Sway

For click generation, Sway uses `UInputBackend`.

For global hotkeys, `HotkeyBackendFactory` prefers `SwayHotkeyBackend` when the session is Wayland and `XDG_CURRENT_DESKTOP` contains `SWAY`.

`SwayHotkeyBackend` converts the hotkey to a Sway `bindsym` string:

- Ctrl -> `Ctrl`
- Alt -> `Mod1`
- Super/Meta -> `Mod4`
- Shift -> `Shift`
- Single-character keys are lowercased.

It registers the binding at runtime using `swaymsg`:

```text
bindsym <key> exec "<exe>" --toggle
```

The command target is:

- Flatpak: `flatpak run io.github.omniclicker`
- Non-Flatpak: `QCoreApplication::applicationFilePath()`

On stop, it unregisters with:

```text
unbindsym <key>
```

Like GNOME, activation launches `omniclicker --toggle`, and the single-instance socket toggles the already-running GUI process.

### Hyprland

For click generation, Hyprland uses `UInputBackend`.

For global hotkeys, `HotkeyBackendFactory` prefers `HyprlandHotkeyBackend` when either:

- `XDG_CURRENT_DESKTOP` contains `HYPRLAND`, or
- `HYPRLAND_INSTANCE_SIGNATURE` is set.

`HyprlandHotkeyBackend` converts modifiers to Hyprland's modifier names:

- Super/Meta -> `SUPER`
- Ctrl -> `CTRL`
- Alt -> `ALT`
- Shift -> `SHIFT`

It converts keys through `QKeySequence` and lowercases single-character keys. It registers the binding with:

```text
hyprctl keyword bind "<mods>, <key>, exec, <exe> --toggle"
```

On stop, it unregisters with:

```text
hyprctl keyword unbind "<mods>, <key>"
```

Inside Flatpak, it uses `flatpak-spawn --host hyprctl ...` and preserves `HYPRLAND_INSTANCE_SIGNATURE` by passing it as an environment override.

As with GNOME and Sway, activation launches a new `--toggle` process and relies on the single-instance socket.

## 6. Global Shortcuts

### Common Hotkey Model

The shared hotkey model is in `src/hotkeys/Hotkey.h`:

```cpp
struct Hotkey {
    int key = Qt::Key_F6;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    QString normalized = QStringLiteral("F6");
};
```

`parseHotkey()` supports:

- Ctrl, Control
- Alt
- Shift
- Super, Meta, Win
- F1 through F24
- Letters and digits
- Space, Tab, Backtab, Insert, Delete, Backspace, Enter, Escape
- Navigation keys
- A set of punctuation keys, including shifted forms like `?`, `_`, `+`, `|`

`normalizeHotkey()` orders modifiers as Ctrl, Alt, Shift, Super, then the key.

### Hotkey Manager Flow

`MainWindow::registerHotkey()` parses `settings_.hotkey`, updates the text field to the normalized string, configures the focused-window fallback `QShortcut`, and calls `hotkeys_.setHotkeyText(settings_.hotkey)`.

`GlobalHotkeyManager::setHotkeyText()`:

1. Parses the hotkey.
2. Calls `stop()` on the existing backend.
3. Iterates over `createHotkeyBackendCandidates()`.
4. Calls `candidate->start(hotkey, callback, &startError)`.
5. Stores the first successful backend.
6. Emits status or an aggregated error.

For in-process backends, the callback is queued back onto the Qt object using `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`, then `GlobalHotkeyManager::activated` is emitted.

The focused-window fallback is a `QShortcut` owned by `MainWindow`. It works only while the application has focus, but remains useful if global registration fails.

### X11 Global Shortcuts

`X11HotkeyBackend` uses X11 passive key grabs:

1. `XOpenDisplay(nullptr)`.
2. Convert Qt key to X11 `KeySym`.
3. Convert to keycode with `XKeysymToKeycode()`.
4. Convert modifiers to X masks: Control, Mod1, Shift, Mod4.
5. Call `XGrabKey()` on the root window for the base modifiers plus ignored lock states:
   - no lock
   - Caps Lock
   - Num Lock
   - Caps Lock + Num Lock

It starts a polling thread that checks `XPending()`, reads events with `XNextEvent()`, filters `KeyPress`, keycode, and active modifiers, then invokes the callback with a 250 ms debounce.

`stop()` joins the thread and ungrabs the key combinations with `XUngrabKey()`.

On Wayland with XWayland available, this backend may be tried as a fallback. Its `limitation()` warns that XWayland hotkeys may not be seen when native Wayland windows are focused.

### KDE KGlobalAccel

`KdeGlobalAccelBackend` integrates with KDE's native global shortcut system through KF6GlobalAccel. It only compiles when `find_package(KF6GlobalAccel QUIET)` succeeds, in which case CMake adds the source file, links `KF6::GlobalAccel`, and defines `HAVE_KGLOBALACCEL=1`.

The backend registers a `QAction` as a KDE global action. KDE owns the global shortcut policy, conflict handling, and activation. Omni Clicker receives activation as `QAction::triggered`.

### xdg-desktop-portal GlobalShortcuts

`PortalHotkeyBackend` uses D-Bus to talk to:

- Service: `org.freedesktop.portal.Desktop`
- Path: `/org/freedesktop/portal/desktop`
- Interface: `org.freedesktop.portal.GlobalShortcuts`

Startup checks:

1. Connect to the session bus.
2. Optionally call `org.freedesktop.host.portal.Registry.Register` with app id `io.github.omniclicker` if that interface is present.
3. Verify the portal service is registered.
4. Create a `QDBusInterface` for `GlobalShortcuts`.
5. Introspect the portal object and require `CreateSession` and `BindShortcuts`.

Registration flow:

1. Subscribe to the portal `Activated` signal.
2. Call `CreateSession` with handle tokens.
3. Wait in a local `QEventLoop` for the asynchronous request response, with a 15 second timeout.
4. On session creation, subscribe to session `Closed` and `ShortcutsChanged`.
5. Call `BindShortcuts` with one shortcut id, `toggle`, plus description and optional `preferred_trigger`.
6. Mark success when `BindShortcuts` returns response code 0.

Activation flow:

- `handleActivated()` checks that the session handle and shortcut id match.
- If active, it invokes the stored callback.

This backend is used as a Wayland fallback and may show compositor/user confirmation dialogs depending on desktop policy.

### GNOME, Sway, Hyprland Shortcut Behavior

GNOME, Sway, and Hyprland backends do not call the callback directly on activation. They configure the desktop/compositor to run `omniclicker --toggle`. That command enters `main()`, detects the existing primary process through `QLocalSocket`, sends `TOGGLE`, and exits.

The `Callback` stored in these backend classes is currently not used after registration. That is not a runtime bug because activation is externalized through the command, but it is a design distinction from X11, KDE, portal, and evdev.

### evdev Fallback

`EvdevHotkeyBackend` reads raw Linux input events:

1. Opens every readable `/dev/input/event*` file.
2. Starts a thread using `poll()` with a 100 ms timeout.
3. Tracks pressed keys in a `std::set<unsigned short>`.
4. Converts the target Qt key to an evdev key code.
5. On target key press, checks exact modifier state and calls the callback with a 250 ms debounce.

This backend requires read permission for input event devices. It is powerful and sensitive because it can observe keyboard events system-wide.

## 7. Permissions and Security

### Native X11

X11 input injection requires access to the X11 display and the XTest extension. No root access is needed. However, any process connected to the user's X server with XTest can synthesize input globally, which is why this is inherently trusted-user functionality.

X11 global hotkeys use passive grabs on the root window. They can fail if another application already owns the same key combination.

### Native Wayland/uinput

Wayland click injection requires write access to `/dev/uinput` and the `uinput` kernel module. The code opens `/dev/uinput` directly; if permission is missing, `UInputBackend::initialize()` returns an error telling the user to load the module and grant write permission.

This is privileged in the sense of device access, not in the sense of running the app as root. A typical setup would use group membership or udev rules to grant trusted users access to `/dev/uinput`.

Because uinput creates a virtual hardware device, the compositor sees events as input-device events rather than Wayland client requests. This bypasses the normal Wayland restriction that untrusted clients cannot inject global input.

### Wayland Global Hotkeys

Permissions vary by backend:

- KDE: D-Bus access to `org.kde.kglobalaccel`.
- Portal: session D-Bus access to `org.freedesktop.portal.Desktop`.
- GNOME: ability to run `gsettings` and modify the user's GNOME custom keybindings.
- Sway: ability to run `swaymsg`.
- Hyprland: ability to run `hyprctl`.
- evdev: read access to `/dev/input/event*`.

The evdev fallback has the broadest security implications because it reads raw keyboard input events.

### Flatpak Requirements

The Flatpak manifest `io.github.omniclicker.yml` declares:

- `--share=ipc`
- `--socket=x11`
- `--socket=wayland`
- `--device=all`
- `--talk-name=org.kde.kglobalaccel`
- `--filesystem=xdg-run/dconf`
- `--filesystem=~/.config/dconf:ro`
- `--talk-name=ca.desrt.dconf`
- `--env=DCONF_USER_CONFIG_DIR=.config/dconf`
- `--talk-name=org.freedesktop.Flatpak`

The key permission for Wayland clicking is `--device=all`, because the app needs `/dev/uinput`. The manifest comment and `flatpak/README.md` both note that host udev permissions are still required.

`org.freedesktop.Flatpak` talk permission is used so the app can invoke host tools with `flatpak-spawn --host`, specifically `gsettings`, `swaymsg`, and `hyprctl`.

## 8. Background Operation

The process remains alive after the main window closes because `main()` calls `QApplication::setQuitOnLastWindowClosed(false)`.

`MainWindow::setupTray()` creates a `QSystemTrayIcon` with a context menu:

- Show Omni Clicker
- Start clicking / Stop clicking
- Quit

If `settings_.keepRunningInBackground` is true and the tray icon is visible, `MainWindow::closeEvent()` hides the window, ignores the close event, and shows a tray notification. The click engine and hotkey backend continue running.

If background mode is off, closing the window stops the engine, stops hotkeys, accepts the close event, and schedules `qApp->quit()` with `QTimer::singleShot(0, ...)`.

The click engine runs on a `std::thread`, not a Qt thread. It communicates back to the GUI through Qt signals. The global hotkey system may also use background threads:

- `X11HotkeyBackend` has its own X event polling thread.
- `EvdevHotkeyBackend` has its own input polling thread.
- Portal and KDE use Qt/D-Bus/action callbacks in the Qt event loop.
- GNOME/Sway/Hyprland rely on external command execution through `--toggle`.

## 9. Code Structure

### Build and Packaging

`CMakeLists.txt`

- Defines the `omniclicker` C++17 executable.
- Finds Qt6 Widgets, DBus, Network; X11; Xtst; optional KF6GlobalAccel.
- Enables Qt automoc/autouic/autorcc.
- Adds backend and hotkey source files.
- Adds KDE backend only when KF6GlobalAccel is found.
- Defines build metadata macros: `APP_VERSION`, `BUILD_TYPE`, `GIT_HASH`, `BUILD_TIMESTAMP`.
- Installs the binary, desktop file, metainfo, and icon.

`data/io.github.omniclicker.desktop`

- Desktop launcher for `omniclicker`.
- Provides the desktop id used by Qt and portal-related code.

`data/io.github.omniclicker.metainfo.xml`

- AppStream metadata. Note: it currently says `<project_license>MIT</project_license>`, while `README.md` says GPL-3.0-or-later and the repository contains `LICENSE`. That inconsistency should be resolved.

`io.github.omniclicker.yml`

- Flatpak manifest and sandbox permissions.

`flatpak/README.md`

- Explains Flatpak permissions and local build commands.

### Core Application

`src/main.cpp`

- Initializes Xlib threading and Qt application metadata.
- Parses `--start-minimized` and `--toggle`.
- Implements single-instance IPC with `QLocalSocket`/`QLocalServer`.
- Constructs and shows `MainWindow`.
- Starts the Qt event loop.

`src/MainWindow.h` and `src/MainWindow.cpp`

- Main GUI class.
- Owns `ClickSettings settings_`, `ClickEngine engine_`, and `GlobalHotkeyManager hotkeys_`.
- Builds the complete Qt Widgets UI.
- Loads settings into controls, persists changes, validates custom keys.
- Starts/stops clicking.
- Registers global hotkeys and focused-window fallback shortcut.
- Manages tray icon/background mode.

`src/ClickEngine.h` and `src/ClickEngine.cpp`

- Threaded click loop.
- Selects and initializes an `InputBackend`.
- Emits runtime state, click count, backend info, and errors.
- Implements absolute-deadline timing and optional random jitter.

`src/Types.h` and `src/Types.cpp`

- Defines enums and `ClickSettings`.
- Converts interval units, mouse buttons, click types, and position modes to/from strings and integers.
- Implements interval conversion to milliseconds.

`src/Config.h` and `src/Config.cpp`

- Loads and saves `ClickSettings` with `QSettings` INI storage.
- Defines the config path.

### Input Backends

`src/backends/InputBackend.h`

- Abstract interface for click injection.
- Defines `BackendCapabilities`.
- Declares `detectedSessionType()` and `createInputBackend()`.

`src/backends/BackendFactory.cpp`

- Detects or forces input backend selection.
- Maps X11 to `X11Backend`, Wayland to `UInputBackend`.

`src/backends/X11Backend.h` and `src/backends/X11Backend.cpp`

- Implements X11/XTest click and key injection.
- Supports current cursor, fixed-coordinate pointer warp, and cursor query internally.
- Converts Qt keys/modifiers to X11 keysyms.

`src/backends/UInputBackend.h` and `src/backends/UInputBackend.cpp`

- Implements virtual input through `/dev/uinput`.
- Creates an `Omni Clicker Virtual Input` device.
- Injects mouse buttons and custom keys as Linux input events.
- Does not support pointer position capture or fixed-coordinate clicking.

### Hotkey System

`src/hotkeys/Hotkey.h` and `src/hotkeys/Hotkey.cpp`

- Defines `Hotkey`.
- Parses text to hotkeys.
- Converts `QKeyEvent` to normalized hotkey text.
- Converts hotkeys to `QKeySequence`.

`src/hotkeys/HotkeyCaptureEdit.h` and `src/hotkeys/HotkeyCaptureEdit.cpp`

- `QLineEdit` subclass for recording hotkeys/custom keys.
- Emits recording and captured-hotkey signals.

`src/hotkeys/HotkeyBackend.h`

- Abstract global hotkey backend interface.
- Defines `start()`, `stop()`, `name()`, and `limitation()`.

`src/hotkeys/HotkeyBackendFactory.cpp`

- Selects global hotkey backend candidates.
- Supports forced selection with `OMNICLICKER_HOTKEY_BACKEND`.
- Orders candidates based on session and desktop environment.

`src/hotkeys/HotkeyManager.h` and `src/hotkeys/HotkeyManager.cpp`

- Parses the configured hotkey.
- Tries backend candidates until one starts.
- Emits activation/status/error signals to `MainWindow`.

`src/hotkeys/X11HotkeyBackend.h` and `src/hotkeys/X11HotkeyBackend.cpp`

- X11 passive key grab implementation.
- Runs a polling event loop thread.
- Debounces activation.

`src/hotkeys/KdeGlobalAccelBackend.h` and `src/hotkeys/KdeGlobalAccelBackend.cpp`

- Optional KDE KGlobalAccel integration.
- Registers a `QAction` as a KDE global shortcut.

`src/hotkeys/PortalHotkeyBackend.h` and `src/hotkeys/PortalHotkeyBackend.cpp`

- xdg-desktop-portal GlobalShortcuts implementation.
- Uses Qt D-Bus, portal sessions, request responses, and activation signals.

`src/hotkeys/GnomeHotkeyBackend.h` and `src/hotkeys/GnomeHotkeyBackend.cpp`

- Configures GNOME custom keybindings through `gsettings`.
- Makes activation run `omniclicker --toggle`.

`src/hotkeys/SwayHotkeyBackend.h` and `src/hotkeys/SwayHotkeyBackend.cpp`

- Registers a runtime Sway `bindsym` through `swaymsg`.
- Makes activation run `omniclicker --toggle`.

`src/hotkeys/HyprlandHotkeyBackend.h` and `src/hotkeys/HyprlandHotkeyBackend.cpp`

- Registers a runtime Hyprland bind through `hyprctl keyword bind`.
- Makes activation run `omniclicker --toggle`.

`src/hotkeys/EvdevHotkeyBackend.h` and `src/hotkeys/EvdevHotkeyBackend.cpp`

- Raw `/dev/input/event*` fallback hotkey monitor.
- Polls keyboard devices and detects exact modifier state.

## 10. Development Notes

### Design Decisions and Tradeoffs

Input injection and hotkey registration are intentionally separate. This is necessary because Wayland click injection is solved through uinput, while Wayland global hotkeys are compositor/desktop-specific.

The input backend is created inside `ClickEngine::run()` rather than kept globally. That keeps backend lifetime scoped to active clicking and lets startup show capability information with a temporary backend in `MainWindow::refreshBackendCapabilities()`. The tradeoff is repeated backend initialization on every clicker start, including `/dev/uinput` virtual device creation on Wayland.

The timing loop uses absolute deadlines and skips missed periods. That is a good choice for an autoclicker because slow backend calls do not create bursts of delayed clicks.

GNOME/Sway/Hyprland hotkeys use external `--toggle` commands. This avoids needing compositor-specific event subscriptions, but it depends on the single-instance socket and on the executable/Flatpak command being callable from the host environment.

The portal backend is implemented but is not the first choice on GNOME/Sway/Hyprland. That appears intentional because portal support and UX vary by compositor, and the desktop-specific integrations can be more predictable.

### Weaknesses and Unclear Areas

Fixed-position clicking exists in `ClickSettings`, `Config`, and `X11Backend`, but the GUI forces `PositionMode::CurrentCursor` and does not expose fixed coordinate controls. This is a partially implemented feature.

Cursor capture exists in the `InputBackend` interface and in `X11Backend::currentPosition()`, but no current GUI path uses it.

The README advertises "scroll wheel mouse click support"; the code implements middle-click/wheel-press, not scroll-wheel movement. If actual scroll up/down is intended, the code needs new button/options and backend mappings.

The Flatpak/AppStream metadata says MIT, while the README says GPL-3.0-or-later. The license metadata should be made consistent.

`Qt6::Network` is linked but no current source appears to use Qt Network APIs.

`UInputBackend` enables `EV_REL`, `REL_X`, and `REL_Y`, but does not emit pointer movement. If relative movement is planned, this is preparatory; otherwise it is unnecessary.

The hotkey backend classes for GNOME, Sway, and Hyprland store `callback_` but do not use it after registration. That is expected with the external `--toggle` design, but the member can mislead future readers.

`GnomeHotkeyBackend::stop()` removes the GNOME custom shortcut when the backend stops. Since `GlobalHotkeyManager::setHotkeyText()` calls `stop()` before re-registering and `MainWindow::~MainWindow()` stops hotkeys on exit, the custom shortcut is not persistent across app runs unless the app is running. That matches the current architecture but may surprise users who expect a desktop shortcut registration to persist.

The portal backend contains very verbose D-Bus logging and comments that look like active debugging notes. That may be useful during development but noisy for production.

### Future Wayland Improvements

For click generation, future Wayland support could add desktop-specific trusted input paths where available, while keeping uinput as a generic fallback. The current code already has a clean `InputBackend` interface for adding this.

Possible backend additions:

- A libei/libeis based input backend if target compositors expose a trusted remote-input path suitable for this app.
- A compositor-specific KDE input method if KDE provides a supported API for trusted synthetic pointer events.
- Better uinput setup diagnostics, including detecting missing module, missing write permission, and Flatpak device exposure separately.
- Real scroll-wheel support:
  - X11: buttons 4/5 for vertical wheel and 6/7 for horizontal, depending on server mapping.
  - uinput: `EV_REL` with `REL_WHEEL` and possibly `REL_HWHEEL`, followed by `SYN_REPORT`.
- UI support for fixed-position clicking on X11, with Wayland controls disabled or explained based on `BackendCapabilities`.
- A startup/capability panel that distinguishes click backend from hotkey backend. Today the visible backend label is for input injection, while hotkey status is shown through status/limitation messages.

For global shortcuts, the current layered strategy is practical:

1. Native desktop API where reliable: KDE KGlobalAccel.
2. Compositor configuration where accepted: GNOME custom shortcut, Sway `bindsym`, Hyprland `bind`.
3. xdg-desktop-portal where available.
4. evdev as a privileged fallback.

The main improvement would be clearer user-facing diagnostics showing which hotkey backend was selected and whether activation is in-process or command-based.
