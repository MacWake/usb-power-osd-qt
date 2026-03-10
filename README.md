# USB Power OSD Qt

A cross-platform Qt6 application for monitoring USB power consumption with on-screen display (OSD) capabilities. This is
the Qt6 port of the original [USB Power OSD](https://github.com/tlamy/usb-power-osd) project.

## Features

- **Real-time USB power monitoring** - Voltage, Current, Power, and Energy consumption (optional)
- **Cross-platform support** - Linux, Windows, and macOS
- **Multiple connection methods** - Bluetooth Low Energy and Serial/USB
- **Auto-discovery** - Automatic detection of compatible devices
- **Modern Qt6 interface** - Native look and feel on all platforms

## Supported Devices

Compatible with USB Power OSD V2-BLE devices that support:

- Bluetooth Low Energy communication
- Serial/USB communication
- Real-time power measurement data

## Usage

### Display

The main window shows real-time power measurements in two rows:

- **Top row** (large, primary font): Voltage (left) and Current (right)
- **Second row** (smaller, secondary font): Power (left), Energy in Wh (center, optional), Min–Max Current range (right)
- **Graph** (bottom): Current history graph, visible when the window is tall enough

Values show the maximum reading across the last 3 samples to reduce noise.

### Connecting a Device

On first launch (or when no device is remembered), the device selection dialog opens automatically. You can connect via:

- **Bluetooth Low Energy** – auto-scans for compatible devices
- **Serial/USB** – select the serial port from a list

The last used device is remembered and reconnected automatically on the next launch. Use **File → Change Device** to
switch devices.

### Menu Reference

#### File Menu

| Item               | Shortcut | Description                                                                |
|--------------------|----------|----------------------------------------------------------------------------|
| Settings           |          | Open the settings dialog                                                   |
| Change Device      |          | Open the device selection dialog                                           |
| Toggle Energy      | `E`      | Show or hide the energy (Wh) display                                       |
| Set base current   | `D`      | Subtract the current measured value from all future readings (zero offset) |
| Reset base current | `X`      | Remove the current offset                                                  |
| Reset History      | `R`      | Clear all measurement history and min/max records                          |
| Audio Output       | `A`      | Toggle audible feedback (see below)                                        |
| Exit               |          | Quit the application                                                       |

### Keyboard Shortcuts

| Key | Action                                            |
|-----|---------------------------------------------------|
| `E` | Toggle energy display                             |
| `R` | Reset measurement history                         |
| `D` | Set base current (zero offset to current reading) |
| `X` | Reset base current offset                         |
| `A` | Toggle audio output                               |

### Mouse Interactions

- **Double-click** on the min/max current label to reset measurement history

### Settings

Open **File → Settings** to configure:

- **Primary Font** – font used for the large voltage and current values
- **Secondary Font** – font used for power, energy, and min/max current
- **Min. historic current** – minimum current threshold (in mA) for recording measurements into history
- **Colors** – background color, text color, and per-voltage-level graph colors (5V, 9V, 15V, 20V, 28V, 36V, 48V)

### Audio Feedback

When audio output is enabled (**File → Audio Output** or `A`), the app generates a continuous tone:

- **Frequency** (400 Hz – 4 kHz): proportional to the square root of current draw, giving higher sensitivity at low
  currents
- **Volume**: increases when current changes rapidly (based on the difference between short-term and long-term standard
  deviation)

Audio output is silent when no device is connected or current is below the minimum threshold.

### Base Current (Zero Offset)

Use **Set base current** (`D`) to subtract the idle/background current from all readings. This is useful when you want
to measure only the incremental current consumed by a device under test, ignoring a constant baseline load (likely when
using an usb-c to proprietary adapter for Dell/HP/Lenovo). 

Use **Reset base current** (`X`) to restore unmodified readings.

## Building

### Prerequisites

- Qt6 (6.5+) with Bluetooth and SerialPort modules
- CMake 3.16+
- C++17 compatible compiler

### Dependencies

#### Ubuntu/Debian

```bash
sudo apt-get install qt6-base-dev qt6-bluetooth-dev qt6-serialport-dev cmake build-essential libbluetooth-dev
```

#### Ubuntu Noble

```bash
sudo apt install qt6-serialbus-dev libxcb-xkb-dev
```

#### Debian

```bash
sudo apt-get install qt6-base-dev qt6-connectivity-dev qt6-serialport-dev cmake build-essential libbluetooth-dev
```

#### macOS

```bash
brew install qt6 cmake

```

#### Windows

Install Qt6 from the official installer and Visual Studio with C++ support.

```bash
git clone https://github.com/tlamy/usb-power-osd-qt.git
cd usb-power-osd-qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```
