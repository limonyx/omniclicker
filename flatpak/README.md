# Flatpak

This directory contains the Flatpak manifest for building and testing omniclicker locally.

### Requested Permissions:
- `--share=ipc` & `--socket=x11`: Required for X11 UI rendering, XWayland compatibility, and **Xtst** click injection on X11 sessions.
- `--socket=wayland`: Required for native Wayland UI rendering.
- `--device=all`: Required to expose `/dev/uinput` to the sandbox. Without this, the generic Wayland click injection backend cannot simulate hardware clicks. Note: Users still need appropriate `udev` rules on the host to allow access to `/dev/uinput`.
- `--talk-name=org.kde.kglobalaccel`: Allows Omni Clicker to communicate with KDE's native global shortcut daemon over D-Bus as a fallback.
- `--filesystem=xdg-run/dconf`, `--filesystem=~/.config/dconf:ro`, `--talk-name=ca.desrt.dconf`, `--env=DCONF_USER_CONFIG_DIR=.config/dconf`: Required to allow the application to silently configure custom global shortcuts directly in GNOME's dconf database.
- `--talk-name=org.freedesktop.Flatpak`: Allows the sandbox to execute host binaries (`hyprctl` and `swaymsg`) so global shortcuts work perfectly on Hyprland and Sway without popups.

### Wayland Support Notes
Because Flatpak tightly restricts host interaction, Omni Clicker automatically adapts its behavior:
- **Global Shortcuts**: On GNOME, the app directly writes to `dconf`. On Sway and Hyprland, it uses `flatpak-spawn --host` to run `swaymsg` and `hyprctl` respectively. This guarantees silent, perfect shortcuts across all major window managers.

## Requirements

Install Flatpak, Flatpak Builder, and the required runtime:

```bash
sudo pacman -S flatpak flatpak-builder

flatpak remote-add --if-not-exists flathub \
https://dl.flathub.org/repo/flathub.flatpakrepo

flatpak install flathub org.kde.Platform//6.6 org.kde.Sdk//6.6
```

## Build

From the project root:

```bash
flatpak-builder build-dir io.github.omniclicker.yml --force-clean
```

## Install

```bash
flatpak-builder --user --install build-dir io.github.omniclicker.yml
```

## Run

```bash
flatpak run io.github.omniclicker
```

## Uninstall

```bash
flatpak uninstall io.github.omniclicker
```

## Notes

omniclicker supports both X11 and Wayland inside the Flatpak sandbox.

Before submitting to Flathub, make sure to:
