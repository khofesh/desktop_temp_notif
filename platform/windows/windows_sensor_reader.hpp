#pragma once
#include "sensor_reader.hpp"

class WindowsSensorReader : public SensorReader {
public:
    // host and port default to LibreHardwareMonitor's built-in web server
    explicit WindowsSensorReader(const char* host = "localhost",
                                 unsigned short port = 8085);

    std::map<std::string, float> read() override;

private:
    std::string    host_;
    unsigned short port_;
};
