# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Goal

Monitor hardware temperatures and send desktop notifications when temperatures exceed configurable thresholds. Runs as a background process. Target platforms: Linux and Windows.

## Tech Stack

- **Language**: C++17
- **Build system**: CMake 3.16+
- **Linux notifications**: `libnotify` C API (preferred) with `notify-send` fork/exec fallback
- **Windows notifications**: Windows Toast Notifications (WinRT) via `DesktopNotificationManagerCompat`
- **Linux sensor reading**: `libsensors` C API (preferred) with `popen("sensors")` + regex fallback
- **Windows sensor reading**: LibreHardwareMonitor HTTP API (`http://localhost:8085/data.json`) via WinHTTP

## Build

```bash
# Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Windows (MSVC)
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### Linux Build Notes

- Detects `libnotify` via PkgConfig; compiles with `-DUSE_LIBNOTIFY` if found, otherwise falls back to `notify-send`
- Detects `libsensors` via `find_path`/`find_library`; compiles with `-DUSE_LIBSENSORS` if found, otherwise falls back to `popen("sensors")` + regex
- Install `lm_sensors-devel` (Fedora) or `libsensors-dev` (Debian/Ubuntu) for the preferred libsensors C API path
- Requires `lm-sensors` package installed; `sensors` binary in PATH only needed for the fallback path

### Windows Build Notes

- Requires LibreHardwareMonitor running as Administrator with web server enabled on port 8085
- Defines: `UNICODE`, `_UNICODE`, `_WIN32_WINNT=0x0A00` (Windows 10/11)
- Links: `winhttp`, `runtimeobject`, `ole32`, `shlwapi`

## Project Structure

```
desktop_temp_notif/
├── CMakeLists.txt
├── sensors_data.md                        # reference lm-sensors output
├── config.linux.conf                      # sample Linux config
├── config.windows.conf                    # sample Windows config
├── config.local.conf                      # local testing config (thresholds at 30°C)
├── data.json                              # sample LibreHardwareMonitor JSON
├── src/
│   ├── main.cpp                           # entry point, polling loop, CLI args
│   ├── sensor_reader.hpp                  # abstract base: SensorReader
│   ├── notifier.hpp                       # abstract base: Notifier, NotificationLevel
│   ├── config.hpp                         # Config / Threshold structs
│   └── config.cpp                         # config loading (file + defaults)
└── platform/
    ├── linux/
    │   ├── linux_sensor_reader.hpp/.cpp   # libsensors C API or popen(`sensors`) fallback
    │   └── linux_notifier.hpp/.cpp        # libnotify or notify-send fallback
    └── windows/
        ├── windows_sensor_reader.hpp/.cpp # WinHTTP + custom JSON parser
        ├── windows_notifier.hpp/.cpp      # WinRT toast notifications
        ├── notification_activator.hpp     # COM activation callback (WRL)
        ├── DesktopNotificationManagerCompat.h/.cpp  # MS compat layer (MIT)
```

## Architecture

- `SensorReader` and `Notifier` are abstract base classes; platform implementations live under `platform/`
- `main.cpp` owns the polling loop: reads sensors, compares against thresholds, sends notifications
- Notification cooldown (default 300 s) suppresses repeated alerts per sensor; escalation (Warning → Critical) overrides cooldown

## Configuration File Format

```ini
poll_interval=30               # seconds between sensor reads
notification_cooldown=300      # seconds before re-notifying same sensor

# Per-sensor thresholds
SYSTIN.warning=70
SYSTIN.critical=80
SMBUSMASTER 0.warning=75
SMBUSMASTER 0.critical=80
```

Pass the config file path as the first CLI argument, or omit for built-in defaults.
Verbose mode: `--verbose` (first arg) prints all readings and skip reasons.

## Default Thresholds

### Linux (lm-sensors)

| Sensor        | Warning (°C) | Critical (°C) |
|---------------|-------------|---------------|
| SYSTIN        | 70          | 80            |
| CPUTIN        | 75          | 80            |
| SMBUSMASTER 0 | 75          | 80            |
| Tctl          | 85          | 95            |
| Tccd1         | 85          | 95            |
| Tccd2         | 85          | 95            |
| temp1         | 100         | 115           |

### Windows (LibreHardwareMonitor)

| Sensor           | Warning (°C) | Critical (°C) |
|------------------|-------------|---------------|
| Core (Tctl/Tdie) | 85          | 95            |
| DIMM #1          | 50          | 80            |
| GPU VR SoC       | 80          | 100           |
| Temperature      | 60          | 79            |

## Sensor Data

`sensors_data.md` contains reference lm-sensors output for this machine:

- **nct6798** (ISA adapter): `SYSTIN`, `CPUTIN`, `AUXTIN*`, `SMBUSMASTER 0`
- **k10temp** (PCI adapter): `Tctl`, `Tccd1`, `Tccd2`
- **r8169** (MDIO adapter): `temp1`

`data.json` contains a reference LibreHardwareMonitor JSON response.

## Skills and learning

- always use C/C++ and cmake skill when developing in this repo.

## Plan mode default

- use plan mode for verification steps, not just building.
- always asking questions to get more clarity
- write detailed specs upfront to reduce ambiguity
- always breakdown tasks with its dependencies

