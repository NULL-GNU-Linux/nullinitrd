#include <string>
#include <iostream>

/*
 * Print a job message.
 * Arguments:
 *  std::string msg - message to print
 */
void log_job(std::string msg) {
    std::cout << "\e[94;1m::\e[0m \e[1m" << msg << "\e[0m\n";
}

/*
 * Print an info message.
 * Arguments:
 *  std::string msg - message to print
 */
void log_info(std::string msg) {
    std::cout << "\e[34m  *\e[0m " << msg << "\e[0m\n";
}

/*
 * Print a warning message.
 * Arguments:
 *  std::string msg - message to print
 */
void log_warning(std::string msg) {
    std::cout << "\e[33m  !\e[0m " << msg << "\e[0m\n";
}

/*
 * Print an error message.
 * Arguments:
 *  std::string msg - message to print
 */
void log_error(std::string msg) {
    std::cout << "\e[31m  x\e[0m " << msg << "\e[0m\n";
}

/*
 * Print a success message.
 * Arguments:
 *  std::string msg - message to print
 */
void log_done(std::string msg) {
    std::cout << "\e[32;1m::\e[0m " << msg << "\e[0m\n";
}
