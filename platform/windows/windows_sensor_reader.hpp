#pragma once
#include "sensor_reader.hpp"

class WindowsSensorReader : public SensorReader {
public:
    WindowsSensorReader();
    ~WindowsSensorReader() override;

    std::map<std::string, float> read() override;

private:
    bool com_initialized_ = false;
};
