#pragma once

#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include "config.hpp"

namespace fs = std::filesystem;

class Generator {
public:
    Generator(const Config& cfg, const std::string& kernel_ver, bool verbose);

    void create_structure();
    void copy_binaries();
    void copy_libraries();
    void copy_modules();
    void create_init();
    void run_hooks();
    void pack(const std::string& output);

private:
    const Config& config;
    std::string kernel_version;
    bool verbose;
    fs::path work_dir;
    std::set<std::string> copied_libs;
    std::vector<std::string> default_modules;

    void create_directory(const fs::path& path);
    void create_symlinks();
    void copy_file(const fs::path& src, const fs::path& dst);
    std::string find_binary(const std::string& name);
    std::vector<std::string> get_dependencies(const std::string& binary);
    void copy_binary_with_deps(const std::string& binary);
    std::vector<std::string> detect_modules();
    void copy_module(const std::string& module);
    std::string get_compression_cmd();
    fs::path get_lib_destination_path(const fs::path& lib_src);
};