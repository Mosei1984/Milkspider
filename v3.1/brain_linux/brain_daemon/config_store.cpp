/**
 * ConfigStore Implementation
 */

#include "config_store.hpp"

#include <fstream>
#include <iostream>

ConfigStore::ConfigStore() {
}

bool ConfigStore::load(const std::string &path) {
    m_config_path = path;

    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    // TODO: Parse JSON configuration
    // For now, use defaults
    // Recommend using nlohmann/json or rapidjson

    std::cout << "[Config] Loaded from " << path << std::endl;
    return true;
}

bool ConfigStore::save(const std::string &path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    // TODO: Write JSON configuration
    file << "{\n";
    file << "  \"v\": \"3.1\",\n";
    file << "  \"default_wakepose\": \"" << default_wakepose << "\",\n";
    file << "  \"eye_backlight\": " << (int)eye_backlight << ",\n";
    file << "  \"eye_auto_mode\": " << (eye_auto_mode ? "true" : "false") << ",\n";
    file << "  \"motion_mode\": \"" << motion_mode << "\",\n";
    file << "  \"interp_q16\": " << (interp_q16 ? "true" : "false") << ",\n";
    file << "  \"enable_eye_service\": " << (enable_eye_service ? "true" : "false") << ",\n";
    file << "  \"enable_scan\": " << (enable_scan ? "true" : "false") << ",\n";
    file << "  \"enable_obstacle_avoidance\": " << (enable_obstacle_avoidance ? "true" : "false") << "\n";
    file << "}\n";

    return true;
}
