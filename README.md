# Desktop Temperature Notification

Monitors hardware temperatures and sends desktop notifications when configurable thresholds are exceeded. Runs as a background process.

Supported platforms: **Linux**, **Windows**

---

## How it works

```
┌─────────────────────┐        ┌──────────────────┐        ┌─────────────────┐
│   Sensor Reader     │──────▶ │   Main Loop       │──────▶ │    Notifier     │
│                     │        │                   │        │                 │
│ Linux:  lm-sensors  │        │ compare vs config │        │ Linux: libnotify│
│ Windows: LHM direct │        │ cooldown tracking │        │ Windows: Toast  │
└─────────────────────┘        └──────────────────┘        └─────────────────┘
```

- Polls sensors every `poll_interval` seconds (default: 30)
- Fires a **Warning** notification when a sensor crosses its warning threshold
- Fires a **Critical** notification when it crosses its critical threshold
- Suppresses repeated notifications for the same sensor/level within `notification_cooldown` seconds (default: 300)
- Escalation (Warning → Critical) always overrides the cooldown

---

## Prerequisites

### Linux

```bash
# Fedora / RHEL
sudo dnf install lm_sensors

# Debian / Ubuntu
sudo apt install lm-sensors

# First-time sensor detection (run once)
sudo sensors-detect --auto
```

A running desktop session with a notification daemon is required (GNOME, KDE, XFCE all provide one).

### Windows

- **Run as Administrator** — required for `LibreHardwareMonitorLib` to load its ring0 driver for direct hardware sensor access
- **.NET runtime** — not required if using the self-contained release binary

No separate LibreHardwareMonitor process or HTTP server is needed. The app reads sensors directly via the embedded library.

---

## Build

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Optional dependencies (preferred paths):
- `libnotify-dev` / `lm_sensors-devel` — enables native C API paths; falls back to `notify-send` and `popen("sensors")` if absent

### Windows

Requires [.NET 10 SDK](https://dotnet.microsoft.com/download/dotnet/10.0).

```bat
cd platform\windows\CSharp
dotnet build
```

To produce a self-contained single `.exe` (no runtime install needed on target machine):

```bat
dotnet publish -c Release -r win-x64 --self-contained true /p:PublishSingleFile=true
```

Binaries:
- Linux: `build/desktop_temp_notif`
- Windows (build): `platform\windows\CSharp\bin\Release\net10.0-windows10.0.19041.0\win-x64\DesktopTempNotif.exe`
- Windows (publish): `platform\windows\CSharp\bin\Release\net10.0-windows10.0.19041.0\win-x64\publish\DesktopTempNotif.exe`

### Pre-built releases

Download ready-to-run binaries from the [Releases](../../releases) page — no build tools required.

---

## Run

### Linux

```bash
# Foreground
./build/desktop_temp_notif

# With config file
./build/desktop_temp_notif config.linux.conf

# Verbose — prints all sensor readings each poll cycle
./build/desktop_temp_notif --verbose

# Background (survives terminal close)
nohup ./build/desktop_temp_notif config.linux.conf &
```

### Windows

Must be run as Administrator (required for hardware sensor access).

```bat
REM Foreground
DesktopTempNotif.exe

REM With config file
DesktopTempNotif.exe config.windows.conf

REM Verbose — prints sensor readings and threshold comparisons
DesktopTempNotif.exe --verbose config.windows.conf

REM Background (no console window)
start /B DesktopTempNotif.exe config.windows.conf
```

**Run at startup via Task Scheduler (recommended):**
1. Open **Task Scheduler** → _Create Basic Task_
2. Trigger: **At log on**
3. Action: **Start a program** → path to `DesktopTempNotif.exe`, argument: path to config file
4. _Properties → General_ → **Run with highest privileges** (needed for ring0 driver)

---

## Verbose mode

Pass `--verbose` (or `-v`) to print all sensor readings and threshold comparisons on each poll cycle. Useful for diagnosing sensor name mismatches or threshold issues.

```
[ok]   Core (Tctl/Tdie): 41.8°C (below warning 85.0°C)
[ok]   DIMM #1: 40.3°C (below warning 50.0°C)
[skip] GPU VR SoC: not found in sensor data
[WARNING] Temperature: 62.5°C (threshold: 60.0°C)
[poll] sleeping 30s...
```

---

## Configuration

Without a config file the built-in defaults are used. Pass a config file path as an argument to override.

Config file format (key=value, lines starting with `#` are comments):

```ini
# seconds between sensor reads
poll_interval=30

# seconds before re-notifying the same sensor at the same alert level
notification_cooldown=300

# per-sensor thresholds in °C
# format: <sensor_name>.warning=<value>  and  <sensor_name>.critical=<value>
SYSTIN.warning=70
SYSTIN.critical=80
```

Sample config files for each platform are included:
- [`config.linux.conf`](config.linux.conf)
- [`config.windows.conf`](config.windows.conf)

### Finding sensor names

**Linux** — run `sensors` and use the names exactly as shown (e.g. `SYSTIN`, `Tctl`, `temp1`).

**Windows** — run `DesktopTempNotif.exe --list-sensors` (as Administrator) to dump all detected temperature sensors and their exact names to `sensors_dump.txt`. Use those names in your config file.

```
DesktopTempNotif.exe --list-sensors
```

Example output:
```
# Hardware                                  Type            Sensor Name                         Temp (°C)
# ---------------------------------------- --------------- ----------------------------------- ---------
  AMD Ryzen 7 PRO 8840U ...                 Cpu             Core (Tctl/Tdie)                    44.3
  SK Hynix - HMCG88AGBSA095N (#1)          Memory          DIMM #1                             42.0
  AMD Radeon 780M Graphics                  GpuAmd          GPU VR SoC                          44.0
  UMIS RPETJ1T24MKP2QDQ                    Storage         Composite Temperature               36.0
  UMIS RPETJ1T24MKP2QDQ                    Storage         Temperature #1                      35.9
  UMIS RPETJ1T24MKP2QDQ                    Storage         Temperature #2                      37.9
```

> Note: LHM may expose NVMe drive metadata (e.g. `Warning Temperature`, `Thermal Sensor High Limit`) as temperature-type sensors. These are drive limits, not actual readings — ignore them.

### Default thresholds

**Linux** (lm-sensors):

| Sensor        | Warning (°C) | Critical (°C) |
|---------------|-------------|---------------|
| SYSTIN        | 70          | 80            |
| CPUTIN        | 75          | 80            |
| SMBUSMASTER 0 | 75          | 80            |
| Tctl          | 85          | 95            |
| Tccd1         | 85          | 95            |
| Tccd2         | 85          | 95            |
| temp1         | 100         | 115           |

**Windows** (LibreHardwareMonitor):

| Sensor           | Warning (°C) | Critical (°C) |
|------------------|-------------|---------------|
| Core (Tctl/Tdie)      | 85          | 95            |
| DIMM #1               | 50          | 80            |
| GPU VR SoC            | 80          | 100           |
| Composite Temperature | 60          | 79            |
| Temperature #1        | 60          | 79            |
| Temperature #2        | 60          | 79            |

> Sensor names vary by hardware. Always verify with `sensors` (Linux) or `--verbose` (Windows).

---

## Troubleshooting

### No notifications appear (Windows)

- Confirm you are running as Administrator.
- Run with `--verbose` and check for `[WARNING]` or `[CRITICAL]` lines — if they appear but no toast shows, check Windows **Focus Assist** / **Do Not Disturb** settings.
- Verify the sensor names in your config match what `--list-sensors` reports.

### No sensors read (Windows)

If `--verbose` shows all sensors as `not found in sensor data`, the ring0 driver failed to load. Ensure the process has Administrator privileges.

### Sensor names not matching

Sensor names differ by hardware and driver version. Use `--verbose` to see exact names being reported, then update your config file to match.

### Linux: notifications not appearing

Ensure a notification daemon is running (`notify-send "test" "hello"` should show a popup).
