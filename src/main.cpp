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

int main(int argc, char* argv[]) {
    Config cfg;
    if (argc > 1) {
        cfg = Config::load(argv[1]);
    } else {
        cfg = Config::default_config();
    }

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

        for (auto& [sensor, temp] : readings) {
            auto thresh_it = cfg.thresholds.find(sensor);
            if (thresh_it == cfg.thresholds.end()) continue;

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
            notifier->send(sensor, temp, threshold_val, level);
            last_notif[sensor] = {level, now};
        }

        std::this_thread::sleep_for(std::chrono::seconds(cfg.poll_interval_seconds));
    }

    return 0;
}
