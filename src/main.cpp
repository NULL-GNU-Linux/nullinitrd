#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include "config.hpp"
#include "generator.hpp"
#include "hooks.hpp"
#include "utils.hpp"
namespace fs = std::filesystem;
void print_version() {
    std::cout << ":: nullinitrd v" << VERSION << std::endl;
    std::cout << ":: NULL's Modular Initramfs Generator." << std::endl;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -o, --output FILE    Output initramfs file" << std::endl;
    std::cout << "  -c, --config FILE    Configuration file" << std::endl;
    std::cout << "  -k, --kernel VER     Kernel version" << std::endl;
    std::cout << "  -v, --verbose        Verbose output" << std::endl;
    std::cout << "  -h, --help           Show this help" << std::endl;
    std::cout << "      --version        Show version" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string output_file;
    std::string config_file = "/etc/nullinitrd/config";
    std::string kernel_version;
    bool verbose = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            print_version();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            output_file = argv[++i];
        } else if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_file = argv[++i];
        } else if ((arg == "-k" || arg == "--kernel") && i + 1 < argc) {
            kernel_version = argv[++i];
        }
    }

    if (kernel_version.empty()) {
        kernel_version = utils::get_kernel_version();
    }
    if (output_file.empty()) {
        output_file = "/boot/initrd.img";
    }

    std::cout << ":: nullinitrd" << std::endl;
    std::cout << ":: linux " << kernel_version << std::endl;
    std::cout << ":: output -> " << output_file << std::endl;
    std::cout << ":: building initramfs..." << std::endl;
    try {
        Config cfg(config_file);
        Generator gen(cfg, kernel_version, verbose);
        gen.create_structure();
        gen.copy_binaries();
        gen.copy_libraries();
        gen.copy_modules();
        gen.create_init();
        gen.run_hooks();
        gen.pack(output_file);
        std::cout << ":: initramfs generated successfully: " << output_file << std::endl;
    } catch (const std::exception& e) {
        std::cerr << ":: [!] " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
