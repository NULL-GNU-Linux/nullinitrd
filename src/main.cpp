#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "config.hpp"
#include "generator.hpp"
#include "hooks.hpp"
#include "utils.hpp"
#include "logger/log.hpp"

/* Print the current version of nullinitrd. */
void print_version() {
    log_job("nullinitrd v" VERSION);
    log_job("NULL's Modular Initramfs Generator.");
}

/*
 * Print usage
 * Arguments:
 *  const char* prog - the program name, use with argv[0]
 */
void print_usage(const char* prog) {
    std::cout << "\e[1mnullinitrd\e[0m: initramfs generator" << std::endl << std::endl;

    std::cout << "\e[1mUsage\e[0m: " << prog << " [OPTIONS]" << std::endl;
    std::cout << "\e[1mOptions\e[0m:" << std::endl;
    std::cout << "  -o, --output FILE    Output initramfs file" << std::endl;
    std::cout << "  -c, --config FILE    Configuration file" << std::endl;
    std::cout << "  -k, --kernel VER     Kernel version" << std::endl;
    std::cout << "  -v, --verbose        Verbose output" << std::endl;
    std::cout << "  -h, --help           Show this help" << std::endl;
    std::cout << "      --custom-init    Use a custom init" << std::endl;
    std::cout << "      --version        Show version" << std::endl << std::endl;

    std::cout << "\e[1mCommands\e[0m:" << std::endl;
    std::cout << "  hook-template NAME   Generate hook template" << std::endl << std::endl;

    std::cout << "Authors:" << std::endl;
    std::cout << "  \e[1mneoapps-dev\e[0m <neo@obsidianos.xyz>" << std::endl;
    std::cout << "  \e[1mmostypc123\e[0m  <mostypc123@redroselinux.org>" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string output_file;
    std::string config_file = "/etc/nullinitrd/config";
    std::string kernel_version;
    std::string custom_init = "no";
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
        } else if ((arg == "hook-template") && i + 1 < argc) {
            create_hook_template(argv[++i]);
            std::exit(0);
        } else if (arg == "--custom-init" && i + 1 < argc) {
            custom_init = argv[++i];
        }
    }

    if (kernel_version.empty()) {
        kernel_version = utils::get_kernel_version();
    }
    if (output_file.empty()) {
        output_file = "/boot/initrd.img";
    }

    log_job("nullinitrd");
    log_info("linux " + kernel_version);
    log_info("writing output to " + output_file);
    try {
        log_job("generating initrd");
        Config cfg(config_file);
        Generator gen(cfg, kernel_version, verbose);
        gen.copy_binaries();
        gen.copy_libraries();
        gen.copy_modules();
        gen.create_init(custom_init);
        gen.run_hooks();
        gen.pack(output_file);
        log_done("initramfs generated successfully: " + output_file);
    } catch (const std::exception& e) {
        log_error(e.what());
        return 1;
    }

    return 0;
}
