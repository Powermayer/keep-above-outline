# Keep Above Outline

A KWin effect for KDE Plasma 6 that draws a colored outline around any window
that has the **Keep Above** property enabled, making it easy to spot pinned
windows at a glance.

![Keep Above Outline in action](screenshots/keep-above-outline_sample.gif)

![Configuration dialog](screenshots/keep-above-outline-config.png)

## Features

- Outlines every window marked *Keep Above* with a configurable border.
- Uses the current Plasma accent color by default, or a custom color of your choice.
- Adjustable border width (1–20 px) and corner radius (0–30 px).
- Configurable through System Settings → Window Management → Desktop Effects.

## Supported platforms

- Wayland, X11
- Plasma 6.4–6.7

## Requirements

- KDE Plasma 6 / KWin 6 (Wayland)
- For X11: Plasma 6.4 through 6.7 and the distribution's KWin X11 runtime and
  development package
- Qt 6 (Core, Gui, Widgets, Quick)
- KF6 (CoreAddons, ConfigWidgets, KCMUtils)
- Extra CMake Modules (ECM)
- Vulkan headers (required transitively by KWin 6.7+)
- A C++20 compiler for Wayland; the KWin X11 6.7 headers require C++23
- CMake ≥ 3.20

## Guided build and install

Run this script from the unzipped effect folder without elevated privileges.
The script does not install anything itself. It builds the effect, reports
missing dependencies, and provides the required terminal commands.

```sh
./install.sh
```

## Manual build and install

### Dependencies

**Arch / CachyOS / Manjaro**
```sh
sudo pacman -S base-devel cmake extra-cmake-modules qt6-base kcoreaddons kconfig kconfigwidgets kcmutils vulkan-headers
# Wayland (runtime and headers): kwin
# X11 (runtime and headers): kwin-x11
```

**Debian / Ubuntu (KDE Neon, Kubuntu)**
```sh
sudo apt install build-essential cmake extra-cmake-modules qt6-base-dev qt6-declarative-dev libkf6coreaddons-dev libkf6config-dev libkf6configwidgets-dev libkf6kcmutils-dev libvulkan-dev
# Wayland: kwin-wayland kwin-dev
# X11: kwin-x11 kwin-x11-dev
```

**Fedora**
```sh
sudo dnf install gcc-c++ cmake extra-cmake-modules qt6-qtbase-devel qt6-qtdeclarative-devel kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kconfigwidgets-devel kf6-kcmutils-devel vulkan-headers
# Wayland: kwin kwin-devel
# X11: kwin-x11 kwin-x11-devel
```

**openSUSE Tumbleweed**
```sh
sudo zypper install gcc-c++ cmake kf6-extra-cmake-modules qt6-base-devel qt6-declarative-devel kf6-kcoreaddons-devel kf6-kconfig-devel kf6-kconfigwidgets-devel kf6-kcmutils-devel vulkan-headers
# Wayland: kwin6 kwin6-devel
# X11: kwin6-x11 kwin6-x11-devel
```

### Build and install (Wayland)

```sh
cmake -B build-wayland -S . -DKWIN_BACKEND=WAYLAND -DCMAKE_BUILD_TYPE=Release &&
cmake --build build-wayland &&
sudo cmake --install build-wayland
```

### Build and install (X11)

```sh
cmake -B build-x11 -S . -DKWIN_BACKEND=X11 -DCMAKE_BUILD_TYPE=Release &&
cmake --build build-x11 &&
sudo cmake --install build-x11
```

## Enabling the effect

1. Open **System Settings → Window Management → Desktop Effects**.
2. Find **Keep Above Outline** under *Appearance*.
3. Tick the checkbox to enable it, and use the gear icon to configure the
   color, border width, and corner radius.

## Configuration

| Option           | Default   | Description                                                |
| ---------------- | --------- | ---------------------------------------------------------- |
| Use accent color | `true`    | Follow the current Plasma accent color.                    |
| Custom color     | `#3daee9` | Used when *Use accent color* is disabled.                  |
| Border width     | `3`       | Outline thickness in pixels (1–20).                        |
| Border radius    | `0`       | Corner radius in pixels (0–30).                            |

Settings are stored in `kwinrc` under the `[Effect-keep-above-outline]` group.

## Project layout

- `keepaboveoutline.{h,cpp}` — the KWin effect plugin.
- `keepaboveoutline_config.{h,cpp,ui}` — the System Settings configuration module.
- `keepaboveoutlineconfig.kcfg` — schema for the persisted settings.
- `metadata.json` — KPlugin metadata used by KWin to load the effect.
- `CMakeLists.txt` — backend-selectable build definitions for both plugins.
- `install.sh` — unprivileged dependency checker and multi-backend build helper.

## Release policy

Source releases contain both implementations. If prebuilt binaries are
published, Wayland and X11 artifacts are labeled separately because they are
not interchangeable.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE) for the full text and the SPDX headers in the source files.

## Disclaimer

Parts of this project were written with the assistance of AI.

## Author

Matthias Bauer
