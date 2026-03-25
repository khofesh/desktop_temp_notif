# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Goal

Monitor hardware temperatures via the `sensors` command (lm-sensors), linux libraries or linux environment and send desktop notifications when temperatures exceed configurable thresholds. Target platforms: Linux and Windows.

## Tech Stack

- **Language**: C++
- **Build system**: CMake
- **Linux notifications**: libnotify (via `notify-send`) or `libnotify` C API
- **Windows notifications**: Windows Toast Notifications (WinRT API) or a library such as WinToast
- **Linux sensor reading**: parse stdout of `sensors` (lm-sensors package)
- **Windows sensor reading**: OpenHardwareMonitor/LibreHardwareMonitor COM/WMI interface or equivalent

## Build

```bash
# Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Windows (MSVC)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Project Structure (planned)

```
desktop_temp_notif/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── sensor_reader.hpp / .cpp   # platform-abstracted sensor reading
│   ├── notifier.hpp / .cpp        # platform-abstracted desktop notification
│   └── config.hpp / .cpp          # threshold configuration
├── platform/
│   ├── linux/
│   │   ├── linux_sensor_reader.cpp
│   │   └── linux_notifier.cpp
│   └── windows/
│       ├── windows_sensor_reader.cpp
│       └── windows_notifier.cpp
└── sensors_data.md                # reference sensors output
```

## Sensor Data

The `sensors_data.md` file contains a reference example of `sensors` output for this machine. Key temperature sources:

- **nct6798** (ISA adapter): motherboard sensors — `SYSTIN`, `CPUTIN`, `AUXTIN*`, `SMBUSMASTER 0`
- **k10temp** (PCI adapter): AMD CPU die temps — `Tctl`, `Tccd1`, `Tccd2`
- **r8169** (MDIO adapter): network adapter — `temp1`

On Linux, sensor data is read by running `sensors` (from the `lm-sensors` package) and parsing its stdout. On Windows, an equivalent hardware monitoring library/tool will be needed.

## Default Thresholds

Based on the sensor data in `sensors_data.md` and typical hardware limits:

| Sensor        | Warning (°C) | Critical (°C) |
|---------------|-------------|---------------|
| SYSTIN        | 70          | 80            |
| CPUTIN        | 75          | 80            |
| SMBUSMASTER 0 | 75          | 80            |
| Tctl (k10temp)| 85          | 95            |
| Tccd1/Tccd2   | 85          | 95            |
| r8169 temp1   | 100         | 115           |

Thresholds should be user-configurable (e.g., via a config file or CLI flags).
