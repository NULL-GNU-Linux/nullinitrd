#include "generator.hpp"
#include "hooks.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

Generator::Generator(const Config& cfg, const std::string& kernel_ver, bool v)
    : config(cfg), kernel_version(kernel_ver), verbose(v) {
    char tmpl[] = "/tmp/nullinitrd.XXXXXX";
    char* tmp = mkdtemp(tmpl);
    if (!tmp) {
        throw std::runtime_error(":: [!] failed to create temp directory");
    }
    chmod(tmp, 0755);
    work_dir = tmp;
    create_structure();
    create_symlinks();
    default_modules = {
        "nvme", "nvme-core", "ahci", "sd_mod", "sr_mod",
        "ext4", "btrfs", "xfs", "vfat", "fat",
        "dm-mod", "dm-crypt",
        "raid0", "raid1", "raid456", "md-mod"
    };
}

void Generator::create_directory(const fs::path& path) {
    if (verbose) {
        std::cout << ":: creating dir: " << path << std::endl;
    }
    fs::create_directories(path);
}

void Generator::create_structure() {
    std::cout << ":: creating structure..." << std::endl;
    create_directory(work_dir / "usr/bin");
    create_directory(work_dir / "usr/lib");
    create_directory(work_dir / "usr/lib64");
    create_directory(work_dir / "etc");
    create_directory(work_dir / "dev");
    create_directory(work_dir / "sys");
    create_directory(work_dir / "proc");
    create_directory(work_dir / "run");
    create_directory(work_dir / "tmp");
    create_directory(work_dir / "mnt/root");
    if (config.is_enabled("LVM")) {
        create_directory(work_dir / "etc/lvm");
    }
    if (config.is_enabled("MDADM")) {
        create_directory(work_dir / "etc/mdadm");
    }
}

void Generator::create_symlinks() {
    std::cout << ":: creating symlinks..." << std::endl;
    auto safe_symlink = [this](const char* target, const fs::path& link) {
        if (fs::exists(link) || fs::is_symlink(link)) fs::remove(link);
        fs::create_symlink(target, link);
    };
    safe_symlink("usr/bin", work_dir / "bin");
    safe_symlink("usr/bin", work_dir / "sbin");
    safe_symlink("usr/lib", work_dir / "lib");
    safe_symlink("usr/lib64", work_dir / "lib64");
}

void Generator::copy_file(const fs::path& src, const fs::path& dst) {
    if (verbose) {
        std::cout << ":: copying " << src << " -> " << dst << std::endl;
    }
    fs::create_directories(dst.parent_path());
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
    struct stat st;
    if (stat(src.c_str(), &st) == 0) {
        chmod(dst.c_str(), st.st_mode);
    }
}

std::string Generator::find_binary(const std::string& name) {
    std::vector<std::string> paths = {
        "/usr/local/sbin", "/usr/local/bin",
        "/usr/sbin", "/usr/bin",
        "/sbin", "/bin"
    };
    for (const auto& path : paths) {
        fs::path full_path = fs::path(path) / name;
        if (fs::exists(full_path)) {
            return full_path.string();
        }
    }
    return "";
}

std::vector<std::string> Generator::get_dependencies(const std::string& binary) {
    std::vector<std::string> deps;
    std::string cmd = "ldd " + binary + " 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return deps;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        auto arrow = line.find("=>");
        if (arrow != std::string::npos) {
            auto start = line.find('/', arrow);
            auto end = line.find('(', start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string lib = line.substr(start, end - start);
                lib.erase(lib.find_last_not_of(" \t\n") + 1);
                deps.push_back(lib);
            }
        } else if (line.find('/') != std::string::npos) {
            auto start = line.find('/');
            auto end = line.find('(', start);
            if (end != std::string::npos) {
                std::string lib = line.substr(start, end - start);
                lib.erase(lib.find_last_not_of(" \t\n") + 1);
                deps.push_back(lib);
            }
        }
    }
    pclose(pipe);
    return deps;
}

fs::path Generator::get_lib_destination_path(const fs::path& lib_src) {
    if (lib_src.string().find("/lib64") != std::string::npos ||
        lib_src.string().find("/x86_64") != std::string::npos) {
        return work_dir / "usr/lib64" / lib_src.filename();
    }
    return work_dir / "usr/lib" / lib_src.filename();
}

void Generator::copy_binary_with_deps(const std::string& binary) {
    std::string bin_path = find_binary(binary);
    if (bin_path.empty()) {
        if (verbose) {
            std::cerr << ":: [?] binary not found: " << binary << std::endl;
        }
        return;
    }

    fs::path src(bin_path);
    fs::path dst = work_dir / "usr/bin" / src.filename();
    copy_file(src, dst);

    auto deps = get_dependencies(bin_path);
    for (const auto& dep : deps) {
        if (copied_libs.count(dep) > 0) continue;
        copied_libs.insert(dep);
        fs::path lib_src(dep);
        if (!fs::exists(lib_src)) continue;
        fs::path lib_dst = get_lib_destination_path(lib_src);
        copy_file(lib_src, lib_dst);
        if (fs::is_symlink(lib_src)) {
            auto target = fs::read_symlink(lib_src);
            auto real_lib = lib_src.parent_path() / target;
            if (fs::exists(real_lib) && copied_libs.count(real_lib.string()) == 0) {
                copied_libs.insert(real_lib.string());
                copy_file(real_lib, lib_dst.parent_path() / target.filename());
            }
        }
    }
}

void Generator::copy_binaries() {
    std::cout << ":: copying binaries..." << std::endl;

    if (config.is_enabled("LVM")) {
        copy_binary_with_deps("lvm");
    }
    if (config.is_enabled("MDADM")) {
        copy_binary_with_deps("mdadm");
    }
    if (config.is_enabled("LUKS")) {
        copy_binary_with_deps("cryptsetup");
    }
}

void Generator::copy_libraries() {
}

std::vector<std::string> Generator::detect_modules() {
    std::vector<std::string> modules;
    std::string cmd = "lsmod 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return modules;
    char buffer[512];
    bool first = true;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        if (first) {
            first = false;
            continue;
        }
        std::string line(buffer);
        auto space = line.find(' ');
        if (space != std::string::npos) {
            modules.push_back(line.substr(0, space));
        }
    }
    pclose(pipe);
    return modules;
}

void Generator::copy_module(const std::string& module) {
    std::string mod_path = "/usr/lib/modules/" + kernel_version;
    std::string mod_name = module;
    std::replace(mod_name.begin(), mod_name.end(), '_', '-');

    std::string cmd = "find " + mod_path + " -name '" + mod_name + ".ko*' -o -name '" + module + ".ko*' 2>/dev/null | head -1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return;

    char buffer[512];
    if (fgets(buffer, sizeof(buffer), pipe)) {
        std::string mod_file(buffer);
        mod_file.erase(mod_file.find_last_not_of(" \t\n") + 1);
        if (!mod_file.empty() && fs::exists(mod_file)) {
            fs::path src(mod_file);
            std::string relative_path_str = src.string().substr(mod_path.length());
            if (!relative_path_str.empty() && relative_path_str.front() == '/') {
                relative_path_str = relative_path_str.substr(1);
            }

            std::string dst_path = relative_path_str;
            bool needs_decompress = false;
            std::string decompress_cmd;

            if (dst_path.size() > 4 && dst_path.substr(dst_path.size() - 4) == ".zst") {
                dst_path = dst_path.substr(0, dst_path.size() - 4);
                decompress_cmd = "zstd -d -c";
                needs_decompress = true;
            } else if (dst_path.size() > 3 && dst_path.substr(dst_path.size() - 3) == ".xz") {
                dst_path = dst_path.substr(0, dst_path.size() - 3);
                decompress_cmd = "xz -d -c";
                needs_decompress = true;
            } else if (dst_path.size() > 3 && dst_path.substr(dst_path.size() - 3) == ".gz") {
                dst_path = dst_path.substr(0, dst_path.size() - 3);
                decompress_cmd = "gzip -d -c";
                needs_decompress = true;
            }

            fs::path dst = work_dir / "usr/lib/modules" / kernel_version / dst_path;
            fs::create_directories(dst.parent_path());
            fs::create_directories(work_dir / "mnt/root");
            fs::create_directories(work_dir / "dev");
            fs::create_directories(work_dir / "dev/pts");
            fs::create_directories(work_dir / "proc");
            fs::create_directories(work_dir / "sys");
            if (needs_decompress) {
                if (verbose) {
                    std::cout << ":: decompressing " << src << " -> " << dst << std::endl;
                }
                std::string full_cmd = decompress_cmd + " '" + mod_file + "' > '" + dst.string() + "'";
                system(full_cmd.c_str());
                chmod(dst.c_str(), 0644);
            } else {
                copy_file(src, dst);
            }
        }
    }
    pclose(pipe);
}

void Generator::copy_modules() {
    std::cout << ":: copying kernel modules..." << std::endl;
    create_directory(work_dir / "usr/lib/modules" / kernel_version);

    std::vector<std::string> modules_to_copy;

    if (config.autodetect_modules) {
        auto detected = detect_modules();
        modules_to_copy.insert(modules_to_copy.end(), detected.begin(), detected.end());
    }

    modules_to_copy.insert(modules_to_copy.end(), default_modules.begin(), default_modules.end());
    modules_to_copy.insert(modules_to_copy.end(), config.modules.begin(), config.modules.end());

    for (const auto& mod : modules_to_copy) {
        copy_module(mod);
    }

    std::string dep_src = "/usr/lib/modules/" + kernel_version + "/modules.dep";
    if (fs::exists(dep_src)) {
        copy_file(dep_src, work_dir / "usr/lib/modules" / kernel_version / "modules.dep");
    }

    std::string alias_src = "/usr/lib/modules/" + kernel_version + "/modules.alias";
    if (fs::exists(alias_src)) {
        copy_file(alias_src, work_dir / "usr/lib/modules" / kernel_version / "modules.alias");
    }
}

void Generator::create_init() {
    std::cout << ":: installing init..." << std::endl;

    std::vector<std::string> init_paths = {
        "/usr/share/nullinitrd/init",
        "/usr/local/share/nullinitrd/init",
        "./bin/init"
    };

    fs::path init_src;
    for (const auto& p : init_paths) {
        if (fs::exists(p)) {
            init_src = p;
            break;
        }
    }

    if (init_src.empty()) {
        throw std::runtime_error(":: [!] init binary not found");
    }

    fs::path init_dst = work_dir / "init";
    copy_file(init_src, init_dst);
    chmod(init_dst.c_str(), 0755);
}

void Generator::run_hooks() {
    std::cout << ":: running hooks..." << std::endl;
    HookManager hook_mgr(config, work_dir, kernel_version, verbose);
    for (const auto& hook : config.hooks) {
        hook_mgr.run_hook(hook);
    }
}

std::string Generator::get_compression_cmd() {
    if (config.compression == "gzip") return "gzip -9";
    if (config.compression == "bzip2") return "bzip2 -9";
    if (config.compression == "xz") return "xz -9 --check=crc32";
    if (config.compression == "lz4") return "lz4 -l -9";
    if (config.compression == "zstd") return "zstd -19 -T0";
    if (config.compression == "lzma") return "lzma -9";
    if (config.compression == "none") return "cat";
    return "zstd -19 -T0";
}

void Generator::pack(const std::string& output) {
    std::cout << ":: packing initramfs..." << std::endl;
    std::string cpio_cmd = "cd " + work_dir.string() + " && find . | cpio -o -H newc 2>/dev/null";
    std::string compress_cmd = get_compression_cmd();
    std::string full_cmd = cpio_cmd + " | " + compress_cmd + " > " + output;
    int ret = system(full_cmd.c_str());
    if (ret != 0) {
        throw std::runtime_error(":: [!] failed to pack initramfs");
    }
    fs::remove_all(work_dir);
}
