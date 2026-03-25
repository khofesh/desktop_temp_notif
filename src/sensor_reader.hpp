#pragma once
#include <map>
#include <string>

class SensorReader {
public:
    virtual ~SensorReader() = default;
    // Returns a map of sensor name -> temperature in Celsius
    virtual std::map<std::string, float> read() = 0;
};
