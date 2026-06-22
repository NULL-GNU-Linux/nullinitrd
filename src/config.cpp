#include "config.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>

Config::Config(const std::string& path)
    : compression("zstd"),
      rootfs_type("ext4"),
      init_path("/sbin/init"),
      autodetect_modules(true) {
    parse_file(path);
}

/*
 * Parse a config file, write the defaults to it if not found.
 */
void Config::parse_file(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        mkdir("/etc/nullinitrd", 0755);

        {
            std::ofstream configfile(path);
            configfile << "CONFIG_STATIC=y\n";
            configfile << "CONFIG_DEBUG=n\n";
            configfile << "CONFIG_LTO=y\n";
            configfile << "CONFIG_FEATURE_LVM=n\n";
            configfile << "CONFIG_FEATURE_LUKS=n\n";
            configfile << "CONFIG_FEATURE_MDADM=n\n";
            configfile << "CONFIG_FEATURE_BTRFS=n\n";
            configfile << "CONFIG_FEATURE_ZFS=n\n";
        }

        file.open(path);
    }

    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        if (line.empty() || line[0] == '#') continue;

        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        if (!value.empty() && value[0] == '"' && value.back() == '"') {
            value = value.substr(1, value.length() - 2);
        }

        config_map[key] = value;
    }

    compression = get("COMPRESSION", "zstd");
    rootfs_type = get("ROOTFS_TYPE", "ext4");
    init_path = get("INIT_PATH", "/sbin/init");
    autodetect_modules = get_bool("AUTODETECT_MODULES", false);
    modules = get_list("MODULES");
    hooks = get_list("HOOKS");
    for (const auto& [key, value] : config_map) {
        if (key.find("FEATURE_") == 0 && get_bool(key, false)) {
            features.insert(key.substr(8));
        }
    }
}

std::string Config::get(const std::string& key, const std::string& default_val) const {
    auto it = config_map.find(key);
    return it != config_map.end() ? it->second : default_val;
}

/*
 * Get a boolean value from the config, return a bool.
 * Arguments:
 *  const std::string& key - key to check
 *  bool default_val - use this value if empty
 */
bool Config::get_bool(const std::string& key, bool default_val) const {
    auto val = get(key, "");
    if (val.empty()) return default_val;
    return val == "y" || val == "yes" || val == "true" || val == "1" || val == "on" || val == "enabled" || val == "iguessso";
}

std::vector<std::string> Config::get_list(const std::string& key) const {
    std::vector<std::string> result;
    auto val = get(key, "");
    if (val.empty()) return result;
    std::stringstream ss(val);
    std::string item;
    while (ss >> item) {
        result.push_back(item);
    }
    return result;
}

/* Check if a feature is enabled, return a boolean.
 * Arguments:
 *  const std::string& feature - the feat to check for
 */
bool Config::is_enabled(const std::string& feature) const {
    return features.count(feature) > 0;
}
