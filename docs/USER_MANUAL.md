# USB Power OSD — User Manual

## Overview

USB Power OSD is a cross-platform Qt6 desktop application that monitors USB power consumption from compatible USB power meters, including the **USB Power OSD V2-BLE** and similar devices. It displays live voltage, current, power, and energy, a scrollable current-history graph, and optional audio feedback.

## Main Display

The main window shows real-time measurements in two rows above a current-history graph:

- **Top row (large)**
  - **Left:** Voltage in volts (V)
  - **Right:** Current in amperes (A)
- **Second row (smaller)**
  - **Left:** Power in watts (W)
  - **Center:** Energy in watt-hours (Wh), shown only when enabled
  - **Right:** Minimum–maximum current range recorded since the last reset
- **Graph (bottom):** Current over time. The graph appears when the window is tall enough.

Displayed values are the maximum reading across the last few samples to reduce noise.

## Connecting a Device

When the app starts without a remembered device, the device selection dialog opens automatically. Choose one of the connection methods:

- **Bluetooth Low Energy** — the app scans for compatible BLE devices and connects automatically.
- **Serial/USB** — select the correct serial port from the list.

The last used device is remembered and reconnected automatically on the next launch. To switch devices, use **File → Change Device**.

## Menu Reference

### File Menu

| Menu Item            | Shortcut | Description |
|----------------------|----------|-------------|
| Settings             |          | Open the settings dialog |
| Change Device        |          | Open the device selection dialog |
| Toggle Energy        | `E`      | Show or hide the energy (Wh) display |
| Set base current     | `D`      | Subtract the current measured value from all future readings |
| Reset base current   | `X`      | Remove the current offset |
| Reset History        | `R`      | Clear measurement history and min/max records |
| Audio Output         | `A`      | Toggle audible feedback |
| Exit                 |          | Quit the application |

### View Menu

| Menu Item                | Shortcut | Description |
|--------------------------|----------|-------------|
| Logarithmic Graph Time   |          | Use a logarithmic time scale on the graph x-axis |
| Show Graph Peaks         | `P`      | Show high/low current annotations on the graph |

### Help Menu

| Menu Item | Description |
|-----------|-------------|
| User Manual | Open this manual in your web browser |
| About       | Show application version and credits |

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `E` | Toggle energy display |
| `R` | Reset measurement history |
| `D` | Set base current to current reading |
| `X` | Reset base current offset |
| `A` | Toggle audio output |
| `P` | Toggle graph peak annotations |
| ← / → | Scroll through measurement history |
| `Home` | Jump to the most recent measurement |

## Mouse Interactions

- **Double-click** the min/max current label to reset the measurement history.
- **Resize** the window to show or hide the graph.

## Settings

Open **File → Settings** to customize:

- **Primary Font** — font used for the large voltage and current values
- **Secondary Font** — font used for power, energy, and min/max current
- **Min. historic current** — minimum current threshold (in mA) for recording measurements into history
- **Colors** — background color, text color, and graph colors for each USB PD voltage level (5 V, 9 V, 15 V, 20 V, 28 V, 36 V, 48 V)

## Graph Options

### History Navigation

Use the **Left** and **Right** arrow keys to scroll back and forth through the captured measurement history. Press **Home** to return to the live view.

### Peak Annotations

Enable **View → Show Graph Peaks** to display the highest and lowest current recorded in the visible history.

### Logarithmic Time Scale

Enable **View → Logarithmic Graph Time** to stretch recent history while still showing long-term trends on the same graph.

## Audio Feedback

When enabled (**File → Audio Output** or `A`), the app generates a continuous tone:

- **Frequency** rises with the square root of current draw, so small changes at low currents are easier to hear.
- **Volume** increases when current changes rapidly, highlighting unstable loads.

Audio is silent when no device is connected or current is below the minimum threshold.

## Base Current (Zero Offset)

**Set base current** (`D`) subtracts the currently measured current from all future readings. This is useful when a constant baseline load is present (for example, when using a USB-C to proprietary adapter) and you want to see only the incremental current of the device under test.

**Reset base current** (`X`) restores unmodified readings.

## Tips for Notebook Repair

- Use the graph history to identify current spikes or drops during boot.
- Enable peak annotations to see the highest and lowest current at a glance.
- Use the logarithmic time scale to keep long boot sequences visible while still resolving recent events.
- Set a base current to ignore the adapter or probe baseline and focus on the unit under test.
- Use audio feedback for hands-free monitoring while probing.

## Support

For updates, source code, and issue reports, visit the project repository:

https://github.com/MacWake/usb-power-osd-qt
