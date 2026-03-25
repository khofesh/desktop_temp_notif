#pragma once
#include "sensor_reader.hpp"

class LinuxSensorReader : public SensorReader {
public:
    std::map<std::string, float> read() override;
};
