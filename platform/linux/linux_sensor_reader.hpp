#pragma once
#include "sensor_reader.hpp"

class LinuxSensorReader : public SensorReader {
public:
#ifdef USE_LIBSENSORS
    LinuxSensorReader();
    ~LinuxSensorReader() override;
private:
    bool initialized_ = false;
public:
#endif
    std::map<std::string, float> read() override;
};
