#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>

class Config {
public:
    Config(const std::string& path);

    std::string get(const std::string& key, const std::string& default_val = "") const;
    bool get_bool(const std::string& key, bool default_val = false) const;
    std::vector<std::string> get_list(const std::string& key) const;

    bool is_enabled(const std::string& feature) const;

    std::string compression;
    std::vector<std::string> modules;
    std::vector<std::string> hooks;
    std::set<std::string> features;
    std::string rootfs_type;
    std::string init_path;
    bool autodetect_modules;

private:
    void parse_file(const std::string& path);
    std::map<std::string, std::string> config_map;
};