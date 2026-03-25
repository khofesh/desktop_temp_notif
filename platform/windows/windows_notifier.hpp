#pragma once
#include "notifier.hpp"

class WindowsNotifier : public Notifier {
public:
    void send(const std::string& sensor, float temp, float threshold,
              NotificationLevel level) override;
};
