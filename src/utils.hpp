#pragma once
#include <string>

namespace utils {
    std::string get_kernel_version();
    bool command_exists(const std::string& cmd);
    std::string execute_command(const std::string& cmd);
}
