#pragma once
#include <unistd.h>
#include <cstdio>

static inline void log_init_job(const char *msg) {
    dprintf(STDOUT_FILENO, "\e[94;1m::\e[0m %s\e[0m\n", msg);
}

static inline void log_init_info(const char *msg) {
    dprintf(STDOUT_FILENO, "\e[34;1m  *\e[0m %s\e[0m\n", msg);
}

static inline void log_init_warn(const char *msg) {
    dprintf(STDOUT_FILENO, "\e[33;1m  !\e[0m %s\e[0m\n", msg);
}

static inline void log_init_err(const char *msg) {
    dprintf(STDERR_FILENO, "\e[31;1m  x\e[0m %s\e[0m\n", msg);
}

static inline void log_init_done(const char *msg) {
    dprintf(STDOUT_FILENO, "\e[32;1m  \xe2\x9c\x93\e[0m %s\e[0m\n", msg);
}
