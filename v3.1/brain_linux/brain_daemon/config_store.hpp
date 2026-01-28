#ifndef CONFIG_STORE_HPP
#define CONFIG_STORE_HPP

#include <string>
#include <cstdint>

/**
 * ConfigStore - Persistent configuration storage
 *
 * Stores and loads configuration from JSON file.
 */
class ConfigStore {
public:
    ConfigStore();

    /**
     * Load configuration from file.
     */
    bool load(const std::string &path);

    /**
     * Save configuration to file.
     */
    bool save(const std::string &path);

    // Configuration values
    std::string default_wakepose = "default";
    uint8_t eye_backlight = 180;
    bool eye_auto_mode = true;
    std::string motion_mode = "legacy_prg";
    bool interp_q16 = false;
    bool enable_eye_service = true;
    bool enable_scan = true;
    bool enable_obstacle_avoidance = true;

    // Scan profile
    int scan_min_deg = -60;
    int scan_max_deg = 60;
    int scan_step_deg = 3;
    int scan_rate_hz = 8;

private:
    std::string m_config_path;
};

#endif // CONFIG_STORE_HPP
