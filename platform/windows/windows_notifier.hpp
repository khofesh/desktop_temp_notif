#pragma once
#include "notifier.hpp"

class WindowsNotifier : public Notifier {
public:
    WindowsNotifier();
    ~WindowsNotifier() override;

    void send(const std::string& sensor, float temp, float threshold,
              NotificationLevel level) override;

private:
    bool winrt_initialized_ = false;
};
