#include "linux_sensor_reader.hpp"
#include <cstdio>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

std::map<std::string, float> LinuxSensorReader::read() {
    std::map<std::string, float> result;

    FILE* pipe = popen("sensors 2>/dev/null", "r");
    if (!pipe) {
        std::cerr << "Error: failed to run 'sensors'\n";
        return result;
    }

    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    pclose(pipe);

    // UTF-8 degree sign is 0xC2 0xB0; match lines that contain "°C"
    const std::string degree_c = "\xc2\xb0""C";

    // Pattern: "SensorName:   +38.0°C ..."
    // Name may contain letters, digits, spaces, underscores (e.g. "SMBUSMASTER 0")
    std::regex re(R"(^([A-Za-z][A-Za-z0-9_ ]*?):\s+[+-]?(\d+\.?\d*))");

    std::istringstream ss(output);
    std::string line;
    while (std::getline(ss, line)) {
        // Only process temperature lines
        if (line.find(degree_c) == std::string::npos) continue;

        std::smatch m;
        if (std::regex_search(line, m, re)) {
            std::string name = m[1].str();
            float temp = std::stof(m[2].str());
            result[name] = temp;
        }
    }

    return result;
}
