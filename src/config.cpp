#include "config.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

Config Config::default_config() {
    Config cfg;

#if defined(_WIN32)
    // Sensor names as reported by LibreHardwareMonitor on Windows.
    cfg.thresholds = {
        // AMD CPU die temp
        {"Core (Tctl/Tdie)",     {85.0f, 95.0f}},
        // DDR5 DIMM — high limit 55°C, critical 85°C per SPD
        {"DIMM #1",              {50.0f, 80.0f}},
        // Integrated GPU (Radeon 780M)
        {"GPU VR SoC",           {80.0f, 100.0f}},
        // NVMe — warning 79°C, critical 82°C per SMART
        {"Composite Temperature", {60.0f, 79.0f}},
        {"Temperature #1",       {60.0f, 79.0f}},
        {"Temperature #2",       {60.0f, 79.0f}},
    };
#else
    // Sensor names as reported by lm-sensors on Linux.
    cfg.thresholds = {
        {"SYSTIN",        {70.0f,  80.0f}},
        {"CPUTIN",        {75.0f,  80.0f}},
        {"SMBUSMASTER 0", {75.0f,  80.0f}},
        {"Tctl",          {85.0f,  95.0f}},
        {"Tccd1",         {85.0f,  95.0f}},
        {"Tccd2",         {85.0f,  95.0f}},
        {"temp1",         {100.0f, 115.0f}},
    };
#endif

    return cfg;
}

// Config file format (key=value, lines starting with # are comments):
//   poll_interval=30
//   notification_cooldown=300
//   SYSTIN.warning=70
//   SYSTIN.critical=80
//   SMBUSMASTER 0.warning=75
//   ...
Config Config::load(const std::string& path) {
    Config cfg = default_config();

    std::ifstream f(path);
    if (!f) {
        std::cerr << "Warning: cannot open config '" << path
                  << "', using defaults\n";
        return cfg;
    }

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "poll_interval") {
            cfg.poll_interval_seconds = std::stoi(value);
        } else if (key == "notification_cooldown") {
            cfg.notification_cooldown_seconds = std::stoi(value);
        } else if (key.size() > 8 &&
                   key.substr(key.size() - 8) == ".warning") {
            std::string sensor = key.substr(0, key.size() - 8);
            cfg.thresholds[sensor].warning = std::stof(value);
        } else if (key.size() > 9 &&
                   key.substr(key.size() - 9) == ".critical") {
            std::string sensor = key.substr(0, key.size() - 9);
            cfg.thresholds[sensor].critical = std::stof(value);
        }
    }
    return cfg;
}
