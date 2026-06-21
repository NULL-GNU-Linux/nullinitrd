#include "hooks.hpp"
#include "logger/log.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

HookManager::HookManager(const Config& cfg, const fs::path& work,
                         const std::string& kver, bool v)
    : config(cfg), work_dir(work), kernel_version(kver), verbose(v) {}

void HookManager::run_script(const fs::path& script) {
    log_job(std::string("[#] ") + script.string());
    std::string cmd = "NULLINITRD_WORKDIR=" + work_dir.string() +
                     " NULLINITRD_KERNEL=" + kernel_version +
                     " " + script.string();

    int ret = system(cmd.c_str());
    if (ret != 0) {
        log_warning(std::string("hook ") + script.string() + " exited with code " + std::to_string(ret));
    }
}

/*
 * Find a hook in /usr/[local]/share/nullinitrd/hooks or /etc/nullinitrd/hooks,
 * return false if not found or true if found.
 * Arguments:
 *   const std::string& hook_name - the hook to find
 */
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

/*
 * Run a hook.
 * Arguments:
 *  const std::string& hook_name - name of the hook
 */
void HookManager::run_hook(const std::string& hook_name) {
    if (!find_and_run(hook_name)) {
        log_warning("hook not found: " + hook_name);
    }
}

void create_hook_template(const std::string& hook_name) {
    log_job("Creating hook template");

    std::vector<std::string> hook_paths = {
        "/etc/nullinitrd/hooks",
        "/usr/share/nullinitrd/hooks",
        "/usr/local/share/nullinitrd/hooks"
    };
    std::string hook_dir = "none";

    for (std::string hook_path : hook_paths) {
        if (std::filesystem::exists(hook_path)) {
            hook_dir = hook_path;
            break;
        }
    }

    if (hook_dir == "none") {
        log_error("no hook directory exists. creating /etc/nullinitrd/hooks.");
        try {
            std::filesystem::create_directories("/etc/nullinitrd/hooks");
            hook_dir = "/etc/nullinitrd/hooks";
        } catch (const std::filesystem::filesystem_error& e) {
            log_error("failed to create directory");
            std::exit(1);
        }
    }

    if (std::filesystem::exists(hook_dir + hook_name)) {
        log_error("Hook " + hook_name + " already exists.");
        std::exit(1);
    }

    try {
        std::ofstream hook(hook_dir + hook_name);
        hook << "#!/bin/sh" << std::endl;
        hook << R"(if [ -z "$NULLINITRD_WORKDIR" ]; then)" << std::endl;
        hook << "    echo 'Error: NULLINITRD_WORKDIR not set'" << std::endl;
        hook << "    exit 1" << std::endl;
        hook << "fi" << std::endl << std::endl;
        hook << "# Your hook code here" << std::endl;
    } catch (const std::filesystem::filesystem_error& e) {
        log_error("Failed to write to the hook file");
        std::exit(1);
    }

    log_done("Generated hook template");
}
