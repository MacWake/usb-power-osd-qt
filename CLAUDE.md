# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

USB Power OSD Qt is a cross-platform Qt6/C++17 desktop application that monitors USB power consumption (voltage, current, power, energy) from custom USB Power OSD V2-BLE hardware. It supports both Bluetooth Low Energy and serial/USB connections, displays live values, a current-history graph, and optional audio feedback.

- Build system: **CMake 3.21+**
- Qt version targeted in CI: **6.9.2** (README says 6.5+)
- Required Qt modules: `Core`, `Widgets`, `Bluetooth`, `SerialPort`, `Multimedia`
- Output executable: `USB-Power-OSD` (macOS: `USB-Power-OSD.app`, Windows: `USB-Power-OSD.exe`)

## Common Development Commands

### Build locally

From the repository root:

```bash
# Generic Unix (Linux/macOS) release build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# macOS universal binary
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build build --config Release

# Windows with Visual Studio 2022
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Linux convenience script

`build_linux.sh` configures and builds for the current architecture into `build-linux-<ARCH>` using Unix Makefiles. It hardcodes `CMAKE_PREFIX_PATH=/usr/lib/x86_64-linux-gnu/cmake/Qt6/`, so it assumes system-installed Qt6.

```bash
./build_linux.sh
```

### Override the application version

Pass `-DPROJECT_VERSION_OVERRIDE=X.Y.Z` to CMake. Default version is `2.0.0`. CI uses this to inject the semantic-release version.

### Packaging

CPack is configured per platform. Typical local usage:

```bash
cd build
# Linux
cpack -G DEB
cpack -G RPM
cpack -G TGZ
# macOS
cpack -G DragNDrop
cpack -G TGZ
# Windows
cpack -G WIX
cpack -G ZIP
```

On Windows the WIX package is produced after an install/package target build:

```bash
cmake --build build --config Release --target install
cmake --build build --config Release --target package
```

### Run the app

```bash
# Linux
./build/USB-Power-OSD

# macOS
./build/USB-Power-OSD.app/Contents/MacOS/USB-Power-OSD

# Windows
./build/Release/USB-Power-OSD.exe
```

### Tests and linting

There are **no automated tests** and **no lint/format configuration files** (no `.clang-format`, `.clang-tidy`, etc.) in this repository. Code changes should be verified by building locally and running the application.

## Architecture

### Application flow

- `src/Main.cpp` creates the `QApplication`, registers `PowerData` as a Qt metatype (`qRegisterMetaType<PowerData>()`), instantiates `OsdSettings`, and opens `MainWindow`.
- `MainWindow` owns the user interface and coordinates data display, settings, and device connections.
- `DeviceManager` abstracts device connectivity. It owns a `BluetoothManager` and a `SerialManager` running on a dedicated `QThread`.
- Live data arrives as `PowerData` objects and is forwarded via Qt signals to `MainWindow`, which stores samples in `MeasurementHistory` and refreshes `CurrentGraph` and value labels.

### Key components

| Class | Responsibility |
|-------|----------------|
| `MainWindow` | Main UI: labels, menu bar, status bar, graph widget, audio, keyboard shortcuts, and device-selection orchestration. |
| `DeviceManager` | Mediates between connection backends. Routes `tryConnect()` to BLE or serial, manages mutual-exclusion state, and forwards parsed `PowerData`. |
| `BluetoothManager` | BLE discovery, connection, GATT service/characteristic handling, and JSON/binary parsing for the V2-BLE protocol. |
| `SerialManager` | QSerialPort-based serial discovery and connection. Supports multiple text protocols (`PLD20`, `PLD28`, `MWAKE1`). Runs on a separate thread via `moveToThread()`. |
| `PowerMonitor` | Parses raw device bytes into `PowerData` and accumulates energy (Wh). Currently used mainly as a serial parser; BLE data can also be parsed here. |
| `MeasurementHistory` | Fixed-capacity circular buffer of `PowerData` samples. Provides min/max/median/std-dev and newest-first access for the graph. |
| `CurrentGraph` | Custom `QWidget` that paints the current-history graph using colors from `OsdSettings` based on `PowerDelivery::PD_VOLTS`. |
| `OsdSettings` | Subclass of `QSettings` for persisting fonts, colors, window geometry, display flags, and the last used device. |
| `SettingsDialog` | Font/color/measurement settings editor. |
| `DeviceSelectionDialog` | Modal dialog to choose Bluetooth auto-connect or a serial port. |
| `AudioGenerator` | `QIODevice`-based sine-wave generator driven by live current, connected to `QAudioSink`. |
| `PowerDelivery` | Enum and helpers mapping detected voltage ranges to PD voltage levels and colors. |

### Threading model

- UI code lives on the main thread.
- `SerialManager` lives on `m_serialThread`; `DeviceManager::tryConnect()` uses `QMetaObject::invokeMethod(..., Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, result), ...)` to safely call serial connection methods and receive a return value.
- Bluetooth callbacks run on Qt's BLE event path (main thread). `DeviceManager` ensures that connecting one backend disconnects the other.

### Data pipeline

1. Backend (`BluetoothManager` or `SerialManager`) receives raw bytes.
2. Raw bytes are converted to `PowerData` either inside `BluetoothManager::parseJsonAndEmitPowerData`/`parseJsonToPowerData` (BLE JSON mode) or `PowerMonitor::parseV2BLEPacket` / `SerialManager` parsing.
3. `DeviceManager` emits `powerDataReceived(PowerData)`.
4. `MainWindow::onPowerDataReceived` normalizes the data, pushes it to `MeasurementHistory`, and updates `lastDataRaw`.
5. `MainWindow::updateLabels` (200 ms timer) reads statistical aggregates from `MeasurementHistory` and refreshes labels and graph.

### UI layout

`MainWindow` does **not** use QLayout. Widgets are created as children of the central widget and positioned/ resized manually in `positionWidgets()` based on the central widget size. Label fonts and stylesheets are built from `OsdSettings` values.

### Settings persistence

`OsdSettings` is initialized in `Main.cpp` with organization `"MacWake"` and application `"USB Display"`. Defaults include platform-specific fonts and PD voltage colors. Call `saveSettings()` after mutating fields. Window geometry is restored on startup and saved on move/resize events.

### Device reconnection

- On launch, `MainWindow` tries to reconnect to `settings->last_device` via `connectLastDevice(false)`.
- If the saved device starts with `"ble"`, BLE scanning starts.
- Otherwise the value is treated as a serial port name and `SerialManager::tryConnect` is invoked.
- If reconnection fails, a 1-second `m_reconnect_timer` repeatedly calls `connectLastDevice(true)` until success.
- Selecting a new device via **File → Change Device** stops the reconnect timer.

### CI / Release

GitHub Actions workflow: `.github/workflows/main.yml`.

- Pull requests: quick builds on Linux, Windows, and macOS.
- Pushes to `main`: full release builds producing DEB/RPM/tar.gz (Linux), MSI/zip (Windows), and signed/notarized `.app`/DMG (macOS).
- Versioning is driven by semantic-release (`.releaserc.json`) using conventional commits. The workflow reads the next version with `semantic-release --dry-run` and passes it to CMake as `-DPROJECT_VERSION_OVERRIDE`.
- There is also an older workflow at `.github/actions/BuildUSBPowerOSDQt.yml`; the active build is in `.github/workflows/main.yml`.

### Important implementation notes

- `PowerData` is declared with `Q_DECLARE_METATYPE(PowerData)` so it can be passed through queued signal/slot connections.
- `MainWindow::normalize(PowerData)` applies the `current_diff_ma` zero-offset before storing/displaying values.
- The graph and displayed values use the **maximum** reading across the last 3 samples to reduce noise (`maxValuesLastN(3, ...)`).
- `MeasurementHistory::push` ignores samples whose current is below `settings->min_current`.
- `SerialManager` target VID/PID: `0x0483` / `0x5740` (STMicroelectronics virtual COM port).
- BLE UUIDs are stored as static `QString` constants in `BluetoothManager`.

## Files and folders

- `src/` — all C++ source and headers.
- `resources/` — icons and `resources.qrc` Qt resource file.
- `CMakeLists.txt` — build configuration, install rules, and CPack packaging.
- `Info.plist.in` — macOS bundle template.
- `usbc-power-osd.desktop` / `usb-power-osd.png` — Linux desktop integration.
- `wix-shortcuts.xml` — WiX shortcut configuration for Windows MSI.
- `build/`, `cmake-build-debug/`, `cmake-build-release/`, `build-linux-aarch64/` — typical local build directories (not committed).
