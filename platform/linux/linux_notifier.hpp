#pragma once
#include "notifier.hpp"

class LinuxNotifier : public Notifier {
public:
#ifdef USE_LIBNOTIFY
    LinuxNotifier();
    ~LinuxNotifier() override;
#endif
    void send(const std::string& sensor, float temp, float threshold,
              NotificationLevel level) override;
};
