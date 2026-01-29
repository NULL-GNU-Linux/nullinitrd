#pragma once
#include <string>
#include <filesystem>
#include "config.hpp"

namespace fs = std::filesystem;

class HookManager {
public:
    HookManager(const Config& cfg, const fs::path& work, 
                const std::string& kver, bool verbose);
    
    void run_hook(const std::string& hook_name);
    
private:
    const Config& config;
    fs::path work_dir;
    std::string kernel_version;
    bool verbose;
    
    void run_script(const fs::path& script);
    bool find_and_run(const std::string& hook_name);
};
