#pragma once
#include <map>
#include <string>

struct Threshold {
    float warning;
    float critical;
};

struct Config {
    std::map<std::string, Threshold> thresholds;
    int poll_interval_seconds       = 30;
    int notification_cooldown_seconds = 300; // suppress repeat alerts per sensor

    static Config default_config();
    static Config load(const std::string& path);
};
