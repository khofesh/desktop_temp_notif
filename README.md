# Desktop Temperature Notification

When motherboard temperature exceeds a configurable threshold, send a desktop notification.

Supported platforms:
- Linux
- Windows

Tech stack:
- C++
- CMake
- Linux notifications: `notify-send` (libnotify fallback if available)
- Windows notifications: WinRT Toast API
- Running in the background

---

## Prerequisites

### Linux

```bash
# Fedora / RHEL
sudo dnf install lm_sensors

# Debian / Ubuntu
sudo apt install lm-sensors

# First-time sensor detection
sudo sensors-detect --auto
```

A running desktop session with a notification daemon is required (GNOME, KDE, XFCE all provide one).

### Windows

Install and run [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) or [OpenHardwareMonitor](https://openhardwaremonitor.org/) with **Options > Enable WMI** turned on. The app queries their WMI namespace for temperature data.

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

The binary is at `build/desktop_temp_notif` (Linux) or `build\Release\desktop_temp_notif.exe` (Windows).

---

## Run

**Foreground:**
```bash
./build/desktop_temp_notif
```

**With a custom config file:**
```bash
./build/desktop_temp_notif /path/to/my.conf
```

**Background (survives terminal close):**
```bash
nohup ./build/desktop_temp_notif &
```

---

## Configuration

Without a config file the built-in defaults are used. To customise, create a file and pass it as the first argument.

```ini
# seconds between sensor reads
poll_interval=30

# seconds before re-notifying the same sensor at the same alert level
notification_cooldown=300

# per-sensor thresholds (°C)
SYSTIN.warning=50
SYSTIN.critical=80
CPUTIN.warning=50
CPUTIN.critical=80
Tctl.warning=50
Tctl.critical=95
Tccd1.warning=50
Tccd1.critical=95
Tccd2.warning=50
Tccd2.critical=95
SMBUSMASTER 0.warning=50
SMBUSMASTER 0.critical=80
temp1.warning=60
temp1.critical=115
```

### Default thresholds

| Sensor        | Warning (°C) | Critical (°C) |
|---------------|-------------|---------------|
| SYSTIN        | 50          | 80            |
| CPUTIN        | 50          | 80            |
| SMBUSMASTER 0 | 50          | 80            |
| Tctl          | 50          | 95            |
| Tccd1         | 50          | 95            |
| Tccd2         | 50          | 95            |
| temp1         | 60          | 115           |
