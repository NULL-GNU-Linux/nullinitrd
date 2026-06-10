#pragma once
#include <unistd.h>
#include <cstring>

static inline void log_init_job(const char *msg) {
    write(STDOUT_FILENO, "\e[94;1m::\e[0m ", 13);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, "\e[0m\n", 4);
}

static inline void log_init_info(const char *msg) {
    write(STDOUT_FILENO, "\e[34;1m  *\e[0m ", 12);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, "\e[0m\n", 4);
}

static inline void log_init_warn(const char *msg) {
    write(STDOUT_FILENO, "\e[33;1m  !\e[0m ", 12);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, "\e[0m\n", 4);
}

static inline void log_init_err(const char *msg) {
    write(STDERR_FILENO, "\e[31;1m  x\e[0m ", 12);
    write(STDERR_FILENO, msg, strlen(msg));
    write(STDERR_FILENO, "\e[0m\n", 4);
}

static inline void log_init_done(const char *msg) {
    write(STDOUT_FILENO, "\e[32;1m  \xe2\x9c\x93\e[0m ", 14);
    write(STDOUT_FILENO, msg, strlen(msg));
    write(STDOUT_FILENO, "\e[0m\n", 4);
}
