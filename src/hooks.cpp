#include "hooks.hpp"
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>
HookManager::HookManager(const Config& cfg, const fs::path& work,
                         const std::string& kver, bool v)
    : config(cfg), work_dir(work), kernel_version(kver), verbose(v) {}

void HookManager::run_script(const fs::path& script) {
    std::cout << ":: [#] " << script << std::endl;
    std::string cmd = "NULLINITRD_WORKDIR=" + work_dir.string() + 
                     " NULLINITRD_KERNEL=" + kernel_version +
                     " " + script.string();
    
    int ret = system(cmd.c_str());
    if (ret != 0) {
        std::cerr << ":: [?] hook " << script << " exited with code " << ret << std::endl;
    }
}

bool HookManager::find_and_run(const std::string& hook_name) {
    std::vector<std::string> search_paths = {
        "/etc/nullinitrd/hooks",
        "/usr/share/nullinitrd/hooks",
        "/usr/local/share/nullinitrd/hooks"
    };
    
    for (const auto& path : search_paths) {
        fs::path hook_path = fs::path(path) / hook_name;
        if (fs::exists(hook_path)) {
            struct stat st;
            if (stat(hook_path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
                run_script(hook_path);
                return true;
            }
        }
    }
    
    return false;
}

void HookManager::run_hook(const std::string& hook_name) {
    if (!find_and_run(hook_name)) {
        if (verbose) {
            std::cerr << ":: [?] hook not found: " << hook_name << std::endl;
        }
    }
}
