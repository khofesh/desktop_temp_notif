# Desktop Temperature Notification

Monitors hardware temperatures and sends desktop notifications when configurable thresholds are exceeded. Runs as a background process.

Supported platforms: **Linux**, **Windows**

---

## How it works

```
┌─────────────────────┐        ┌──────────────────┐        ┌─────────────────┐
│   Sensor Reader     │──────▶ │   Main Loop       │──────▶ │    Notifier     │
│                     │        │                   │        │                 │
│ Linux:  lm-sensors  │        │ compare vs config │        │ Linux: notify-  │
│ Windows: LHM HTTP   │        │ cooldown tracking │        │   send / notify │
└─────────────────────┘        └──────────────────┘        │ Windows: WinRT  │
                                                            │   Toast API     │
                                                            └─────────────────┘
```

- Polls sensors every `poll_interval` seconds (default: 30)
- Fires a **Warning** notification when a sensor crosses its warning threshold
- Fires a **Critical** notification when it crosses its critical threshold
- Suppresses repeated notifications for the same sensor/level within `notification_cooldown` seconds (default: 300)

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

Install [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor).

This app reads sensor data from LHM's built-in HTTP server (WMI is unreliable in recent LHM versions).

Setup:
1. **Run LHM as Administrator** — right-click → _Run as administrator_. Required for hardware access.
2. **Options → Remote Web Server → Run** — starts the HTTP server at `http://localhost:8085`.
3. Verify: open `http://localhost:8085/data.json` in a browser. You should see sensor data as JSON.
4. Keep LHM running in the background while `desktop_temp_notif.exe` is active.

On first run the app automatically registers itself as a COM server in the current user's registry so that Windows toast notifications work correctly.

---

## Build

### Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows (MSVC)

```bat
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Binaries:
- Linux: `build/desktop_temp_notif`
- Windows: `build\Release\desktop_temp_notif.exe`

---

## Run

### Linux

```bash
# Foreground
./build/desktop_temp_notif

# With config file
./build/desktop_temp_notif config.linux.conf

# Verbose — prints sensor readings each poll cycle
./build/desktop_temp_notif --verbose

# Background (survives terminal close)
nohup ./build/desktop_temp_notif config.linux.conf &
```

### Windows

```bat
REM Foreground
build\Release\desktop_temp_notif.exe

REM With config file
build\Release\desktop_temp_notif.exe config.windows.conf

REM Verbose — prints sensor readings and threshold comparisons
build\Release\desktop_temp_notif.exe --verbose config.windows.conf

REM Background (no console window)
start /B build\Release\desktop_temp_notif.exe config.windows.conf
```

**Run at startup via Task Scheduler (recommended):**
1. Open **Task Scheduler** → _Create Basic Task_
2. Trigger: **At log on**
3. Action: **Start a program** → path to `desktop_temp_notif.exe`, argument: path to config file
4. _Properties → General_ → **Run only when user is logged on**

---

## Verbose mode

Pass `--verbose` (or `-v`) to print all sensor readings and threshold comparisons on each poll cycle. Useful for diagnosing sensor name mismatches or threshold issues.

```
--- sensor readings (6) ---
  Core (Tctl/Tdie): 41.8 C
  DIMM #1: 40.3 C
  GPU VR SoC: 41.0 C
  Composite Temperature: 36.0 C
  Temperature #1: 35.9 C
  Temperature #2: 36.9 C
[WARNING] Core (Tctl/Tdie) 41.8 C (threshold: 20.0 C)
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

**Windows** — open `http://localhost:8085/data.json` in a browser while LHM is running, or use `--verbose` to see what names the app is reading.

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

| Sensor                | Warning (°C) | Critical (°C) |
|-----------------------|-------------|---------------|
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

- Confirm LHM is running as Administrator with **Remote Web Server** enabled.
- Run with `--verbose` to confirm sensors are being read and thresholds are crossed (look for `[WARNING]` or `[CRITICAL]` lines in the output).
- Check Windows **Focus Assist** / **Do Not Disturb** settings — these can suppress toast notifications.

### Sensor readings (0) — Windows

The HTTP request to LHM failed. The console will print the specific error. Common causes:
- LHM's web server is not enabled (**Options → Remote Web Server → Run**)
- LHM is not running as Administrator

### Sensor names not matching

Sensor names differ by hardware and platform. Use `--verbose` to see the exact names being reported, then update your config file to match.

### Linux: notifications not appearing

Ensure a notification daemon is running (`notify-send "test" "hello"` should show a popup).
