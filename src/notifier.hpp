#pragma once
#include <string>

enum class NotificationLevel { Warning, Critical };

class Notifier {
public:
    virtual ~Notifier() = default;
    virtual void send(const std::string& sensor, float temp, float threshold,
                      NotificationLevel level) = 0;
};
