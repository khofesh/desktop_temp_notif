# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Goal

Monitor hardware temperatures and send desktop notifications when temperatures exceed configurable thresholds. Runs as a background process. Target platforms: Linux and Windows.

## Tech Stack

- **Language**: C++17 (Linux), C# / .NET 10 (Windows)
- **Build system**: CMake 3.16+ (Linux), `dotnet` CLI (Windows)
- **Linux notifications**: `libnotify` C API (preferred) with `notify-send` fork/exec fallback
- **Windows notifications**: Windows Toast via `Microsoft.Toolkit.Uwp.Notifications` NuGet
- **Linux sensor reading**: `libsensors` C API (preferred) with `popen("sensors")` + regex fallback
- **Windows sensor reading**: `LibreHardwareMonitorLib` NuGet (direct hardware access, no separate LHM process needed)

## Build

```bash
# Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Windows
cd platform/windows/CSharp
dotnet build
# or publish single self-contained exe:
dotnet publish -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true
```

### Linux Build Notes

- Detects `libnotify` via PkgConfig; compiles with `-DUSE_LIBNOTIFY` if found, otherwise falls back to `notify-send`
- Detects `libsensors` via `find_path`/`find_library`; compiles with `-DUSE_LIBSENSORS` if found, otherwise falls back to `popen("sensors")` + regex
- Install `lm_sensors-devel` (Fedora) or `libsensors-dev` (Debian/Ubuntu) for the preferred libsensors C API path
- Requires `lm-sensors` package installed; `sensors` binary in PATH only needed for the fallback path

### Windows Build Notes

- Requires .NET 10 SDK installed
- Must run as Administrator (ring0 driver needed by `LibreHardwareMonitorLib` for hardware sensor access)
- No separate LibreHardwareMonitor process or HTTP server needed

## Project Structure

```
desktop_temp_notif/
├── CMakeLists.txt                         # Linux only
├── sensors_data.md                        # reference lm-sensors output
├── config.linux.conf                      # sample Linux config
├── config.windows.conf                    # sample Windows config
├── config.local.conf                      # local testing config (thresholds at 30°C)
├── data.json                              # sample LibreHardwareMonitor JSON
├── src/
│   ├── main.cpp                           # entry point, polling loop, CLI args (Linux)
│   ├── sensor_reader.hpp                  # abstract base: SensorReader
│   ├── notifier.hpp                       # abstract base: Notifier, NotificationLevel
│   ├── config.hpp                         # Config / Threshold structs
│   └── config.cpp                         # config loading (file + defaults)
└── platform/
    ├── linux/
    │   ├── linux_sensor_reader.hpp/.cpp   # libsensors C API or popen(`sensors`) fallback
    │   └── linux_notifier.hpp/.cpp        # libnotify or notify-send fallback
    └── windows/
        ├── CSharp/                        # Windows C# project (active)
        │   ├── DesktopTempNotif.csproj    # net10.0-windows, LHM + Toast NuGet
        │   ├── Program.cs                 # polling loop, CLI args, cooldown logic
        │   ├── SensorReader.cs            # LibreHardwareMonitorLib wrapper
        │   ├── Notifier.cs                # Windows Toast notifications
        │   └── Config.cs                  # INI config parser + Windows defaults
        ├── windows_sensor_reader.hpp/.cpp # legacy C++: WinHTTP + custom JSON parser
        ├── windows_notifier.hpp/.cpp      # legacy C++: WinRT toast notifications
        ├── notification_activator.hpp     # legacy C++: COM activation callback (WRL)
        └── DesktopNotificationManagerCompat.h/.cpp  # legacy C++: MS compat layer (MIT)
```

## Architecture

- **Linux**: `SensorReader` / `Notifier` abstract base classes in C++; implementations under `platform/linux/`; `main.cpp` owns the polling loop
- **Windows**: Self-contained C# project under `platform/windows/CSharp/`; `Program.cs` owns the polling loop; same config format and CLI args as Linux
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
| ------------- | ------------ | ------------- |
| SYSTIN        | 70           | 80            |
| CPUTIN        | 75           | 80            |
| SMBUSMASTER 0 | 75           | 80            |
| Tctl          | 85           | 95            |
| Tccd1         | 85           | 95            |
| Tccd2         | 85           | 95            |
| temp1         | 100          | 115           |

### Windows (LibreHardwareMonitor)

| Sensor           | Warning (°C) | Critical (°C) |
| ---------------- | ------------ | ------------- |
| Core (Tctl/Tdie) | 85           | 95            |
| DIMM #1          | 50           | 80            |
| GPU VR SoC       | 80           | 100           |
| Temperature      | 60           | 79            |

## Sensor Data

`sensors_data.md` contains reference lm-sensors output for this machine:

- **nct6798** (ISA adapter): `SYSTIN`, `CPUTIN`, `AUXTIN*`, `SMBUSMASTER 0`
- **k10temp** (PCI adapter): `Tctl`, `Tccd1`, `Tccd2`
- **r8169** (MDIO adapter): `temp1`

`data.json` contains a reference LibreHardwareMonitor JSON response.

## Skills and learning

- always use C/C++, C-sharp (C#) and cmake skill when developing in this repo.

## Plan mode default

- use plan mode for verification steps, not just building.
- always asking questions to get more clarity
- write detailed specs upfront to reduce ambiguity
- always breakdown tasks with its dependencies
