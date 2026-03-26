#include "linux_sensor_reader.hpp"
#include <iostream>

#ifdef USE_LIBSENSORS

#include <cstdlib>
#include <sensors/sensors.h>

LinuxSensorReader::LinuxSensorReader() {
    if (sensors_init(NULL) != 0) {
        std::cerr << "Error: sensors_init() failed\n";
        initialized_ = false;
    } else {
        initialized_ = true;
    }
}

LinuxSensorReader::~LinuxSensorReader() {
    if (initialized_) {
        sensors_cleanup();
    }
}

std::map<std::string, float> LinuxSensorReader::read() {
    std::map<std::string, float> result;
    if (!initialized_) return result;

    const sensors_chip_name* chip;
    int chip_nr = 0;

    while ((chip = sensors_get_detected_chips(NULL, &chip_nr)) != NULL) {
        const sensors_feature* feature;
        int feat_nr = 0;

        while ((feature = sensors_get_features(chip, &feat_nr)) != NULL) {
            if (feature->type != SENSORS_FEATURE_TEMP) continue;

            const sensors_subfeature* sub =
                sensors_get_subfeature(chip, feature,
                                       SENSORS_SUBFEATURE_TEMP_INPUT);
            if (!sub) continue;

            double value;
            if (sensors_get_value(chip, sub->number, &value) != 0) continue;

            char* label = sensors_get_label(chip, feature);
            if (!label) continue;

            result[std::string(label)] = static_cast<float>(value);
            free(label);
        }
    }

    return result;
}

#else // USE_LIBSENSORS — fall back to popen("sensors") + regex parsing

#include <cstdio>
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

#endif // USE_LIBSENSORS
