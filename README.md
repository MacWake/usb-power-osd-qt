# USB Power OSD Qt

A cross-platform Qt6 application for monitoring USB power consumption with on-screen display (OSD) capabilities. This is the Qt6 port of the original [USB Power OSD](https://github.com/tlamy/usb-power-osd) project.

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
