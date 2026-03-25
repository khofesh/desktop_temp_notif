#include "config.hpp"
#include "notifier.hpp"
#include "sensor_reader.hpp"

#ifdef __linux__
#include "linux_sensor_reader.hpp"
#include "linux_notifier.hpp"
#elif _WIN32
#include "windows_sensor_reader.hpp"
#include "windows_notifier.hpp"
#endif

#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <memory>
#include <thread>

static bool verbose = false;

int main(int argc, char* argv[]) {
    // Usage: desktop_temp_notif [--verbose] [config_file]
    Config cfg;
    std::string config_path;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else {
            config_path = arg;
        }
    }

    cfg = config_path.empty() ? Config::default_config() : Config::load(config_path);

#ifdef __linux__
    std::unique_ptr<SensorReader> reader = std::make_unique<LinuxSensorReader>();
    std::unique_ptr<Notifier>     notifier = std::make_unique<LinuxNotifier>();
#elif _WIN32
    std::unique_ptr<SensorReader> reader = std::make_unique<WindowsSensorReader>();
    std::unique_ptr<Notifier>     notifier = std::make_unique<WindowsNotifier>();
#else
    std::cerr << "Unsupported platform\n";
    return 1;
#endif

    std::cout << "Desktop Temperature Monitor started\n";
    std::cout << "  Poll interval:         " << cfg.poll_interval_seconds << "s\n";
    std::cout << "  Notification cooldown: " << cfg.notification_cooldown_seconds << "s\n";

    struct NotifState {
        NotificationLevel level;
        std::time_t       time;
    };
    std::map<std::string, NotifState> last_notif;

    while (true) {
        auto readings = reader->read();
        std::time_t now = std::time(nullptr);

        if (verbose) {
            std::cout << "\n--- sensor readings (" << readings.size() << ") ---\n";
            for (auto& [s, t] : readings)
                std::cout << "  " << s << ": " << t << " C\n";
        }

        for (auto& [sensor, temp] : readings) {
            auto thresh_it = cfg.thresholds.find(sensor);
            if (thresh_it == cfg.thresholds.end()) {
                if (verbose)
                    std::cout << "  [skip] " << sensor << " — not in config\n";
                continue;
            }

            const Threshold& thresh = thresh_it->second;
            bool should_notify = false;
            NotificationLevel level = NotificationLevel::Warning;

            if (temp >= thresh.critical) {
                level = NotificationLevel::Critical;
                should_notify = true;
            } else if (temp >= thresh.warning) {
                level = NotificationLevel::Warning;
                should_notify = true;
            }

            if (!should_notify) continue;

            // Suppress if same or lower level within cooldown window
            auto it = last_notif.find(sensor);
            if (it != last_notif.end()) {
                bool same_or_lower = (it->second.level == level ||
                    (it->second.level == NotificationLevel::Critical &&
                     level == NotificationLevel::Warning));
                if (same_or_lower &&
                    (now - it->second.time) < cfg.notification_cooldown_seconds) {
                    continue;
                }
            }

            float threshold_val = (level == NotificationLevel::Critical)
                                      ? thresh.critical
                                      : thresh.warning;
            const char* level_str = (level == NotificationLevel::Critical)
                                        ? "CRITICAL" : "WARNING";
            std::cout << "[" << level_str << "] " << sensor
                      << " " << temp << " C (threshold: " << threshold_val << " C)\n";
            notifier->send(sensor, temp, threshold_val, level);
            last_notif[sensor] = {level, now};
        }

        std::this_thread::sleep_for(std::chrono::seconds(cfg.poll_interval_seconds));
    }

    return 0;
}
