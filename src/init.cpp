#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <linux/reboot.h>
#include <string>
#include "logger/log_init.hpp"

static char cmdline[4096];
static char root_dev[256] = "/dev/sda1";
static char root_type[32] = "ext4";
static char root_flags[256] = "ro";
static char init_path[256] = "/sbin/init";
static int root_delay = 0;
static bool verbose = false;

static char modules_to_load[4096] = "";

/*
 * Enter infinite sleep after a critical error occurs
 * TODO: add a recovery shell
 */
static void panic(const char *msg) {
    char buf[512];
    snprintf(buf, sizeof(buf), "PANIC: %s", msg);
    log_init_err(buf);
    log_init_err("dropping to infinite sleep");
    for (;;) sleep(3600);
}

static void do_mount(const char *src, const char *tgt, const char *type, unsigned long flags, const void *data) {
    mkdir(tgt, 0755);
    if (mount(src, tgt, type, flags, data) < 0 && errno != EBUSY) {
        char buf[512];
        snprintf(buf, sizeof(buf), "mount failed: %s (%s)", tgt, strerror(errno));
        log_init_err(buf);
    }
}

static int run_command(const char *cmd, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execv(cmd, argv);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
    }
    return -1;
}

static void parse_cmdline() {
    int fd = open("/proc/cmdline", O_RDONLY);
    if (fd < 0) return;
    ssize_t n = read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    if (n <= 0) return;
    cmdline[n] = '\0';
    if (cmdline[n-1] == '\n') cmdline[n-1] = '\0';

    if (verbose) {
        log_init_info(std::string("cmdline: ").append(cmdline).c_str());
    }

    char *p = cmdline;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *key = p;
        char *val = nullptr;
        while (*p && *p != ' ' && *p != '=') p++;
        if (*p == '=') {
            *p++ = '\0';
            val = p;
            while (*p && *p != ' ') p++;
            if (*p) *p++ = '\0';
        } else if (*p) {
            *p++ = '\0';
        }

        if (strcmp(key, "root") == 0 && val) {
            strncpy(root_dev, val, sizeof(root_dev) - 1);
        } else if (strcmp(key, "rootfstype") == 0 && val) {
            strncpy(root_type, val, sizeof(root_type) - 1);
        } else if (strcmp(key, "rootflags") == 0 && val) {
            strncpy(root_flags, val, sizeof(root_flags) - 1);
        } else if (strcmp(key, "init") == 0 && val) {
            strncpy(init_path, val, sizeof(init_path) - 1);
        } else if (strcmp(key, "rootdelay") == 0 && val) {
            root_delay = atoi(val);
        } else if (strcmp(key, "rw") == 0) {
            strcpy(root_flags, "rw");
        } else if (strcmp(key, "ro") == 0) {
            strcpy(root_flags, "ro");
        } else if (strcmp(key, "rd.debug") == 0 || strcmp(key, "initrd.debug") == 0) {
            verbose = true;
        } else if (strcmp(key, "rd.modules") == 0 && val) {
            strncpy(modules_to_load, val, sizeof(modules_to_load) - 1);
        }
    }
}

/*
 * Load kernel modules.
 */
static void load_modules() {
    log_init_job("loading modules");

    struct utsname uts;
    if (uname(&uts) < 0) {
        log_init_err("uname failed");
        return;
    }

    char moddir[256];
    snprintf(moddir, sizeof(moddir), "/usr/lib/modules/%s", uts.release);

    struct stat st;
    if (stat(moddir, &st) < 0) {
        log_init_info("module dir not found, skipping");
        return;
    }

    static const char *default_modules[] = {
        "nvme", "nvme_core", "ahci", "sd_mod", "sr_mod",
        "ext4", "btrfs", "xfs", "vfat", "fat",
        "usb_storage", "uas", "ehci_hcd", "ehci_pci",
        "xhci_hcd", "xhci_pci", "ohci_hcd", "ohci_pci",
        "dm_mod", "dm_crypt",
        "raid0", "raid1", "raid456", "md_mod",
        nullptr
    };

    int loaded = 0;

    for (int i = 0; default_modules[i]; i++) {
        char *argv[] = {
            (char*)"/usr/bin/modprobe",
            (char*)"-qab",
            (char*)default_modules[i],
            nullptr
        };
        if (run_command("/usr/bin/modprobe", argv) == 0) {
            if (verbose) {
                char buf[256];
                snprintf(buf, sizeof(buf), "loaded: %s", default_modules[i]);
                log_init_info(buf);
            }
            loaded++;
        }
    }

    if (modules_to_load[0]) {
        char mods[4096];
        strncpy(mods, modules_to_load, sizeof(mods) - 1);
        mods[sizeof(mods) - 1] = '\0';

        char *mod = strtok(mods, ",");
        while (mod) {
            char *argv[] = {
                (char*)"/usr/bin/modprobe",
                (char*)"-qab",
                mod,
                nullptr
            };
            if (run_command("/usr/bin/modprobe", argv) == 0) {
                if (verbose) {
                    char buf[256];
                    snprintf(buf, sizeof(buf), "loaded: %s", mod);
                    log_init_info(buf);
                }
                loaded++;
            }
            mod = strtok(nullptr, ",");
        }
    }

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "loaded %d modules", loaded);
        log_init_info(buf);
    }
}

/*
 * TODO: find NVMe support
 */
static char *resolve_device(char *dev) {
    static char resolved[256];

    if (strncmp(dev, "UUID=", 5) == 0 || strncmp(dev, "PARTUUID=", 9) == 0 ||
        strncmp(dev, "LABEL=", 6) == 0) {
        char link[512];
        char *type, *val;

        if (strncmp(dev, "UUID=", 5) == 0) {
            type = (char*)"by-uuid";
            val = dev + 5;
        } else if (strncmp(dev, "PARTUUID=", 9) == 0) {
            type = (char*)"by-partuuid";
            val = dev + 9;
        } else {
            type = (char*)"by-label";
            val = dev + 6;
        }

        snprintf(link, sizeof(link), "/dev/disk/%s/%s", type, val);

        if (verbose) {
            log_init_info(std::string("resolving: ").append(link).c_str());
        }

        for (int i = 0; i < 30; i++) {
            if (access(link, F_OK) == 0) {
                ssize_t len = readlink(link, resolved, sizeof(resolved) - 1);
                if (len > 0) {
                    resolved[len] = '\0';
                    if (resolved[0] != '/') {
                        char tmp[256];
                        snprintf(tmp, sizeof(tmp), "/dev/%s", resolved);
                        strcpy(resolved, tmp);
                    }
                    return resolved;
                }
            }
            log_init_job("waiting for root device...");
            sleep(1);
        }
        log_init_err("failed to resolve device");
    }

    return dev;
}

/*
 * Switch root.
 */
static void switch_root() {
    chdir("/mnt/root");
    mount(".", "/", nullptr, MS_MOVE, nullptr);
    chroot(".");
    chdir("/");
}

int main() {
    log_init_job("nullinitrd");

    do_mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr);
    do_mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr);
    do_mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
    do_mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755");
    mkdir("/dev/pts", 0755);

    parse_cmdline();
    load_modules();

    if (root_delay > 0) {
        char buf[64];
        snprintf(buf, sizeof(buf), "waiting %ds for root device", root_delay);
        log_init_job(buf);
        sleep(root_delay);
    }

    char *dev = resolve_device(root_dev);
    {
        char buf[512];
        snprintf(buf, sizeof(buf), "mounting root: %s (%s)", dev, root_type);
        log_init_job(buf);
    }

    mkdir("/mnt/root", 0755);
    unsigned long mflags = 0;
    if (strstr(root_flags, "ro")) mflags |= MS_RDONLY;

    for (int i = 0; i < 30; i++) {
        if (mount(dev, "/mnt/root", root_type, mflags, nullptr) == 0) break;
        if (i == 29) {
            char buf[256];
            snprintf(buf, sizeof(buf), "mount error: %s", strerror(errno));
            log_init_err(buf);
            panic("failed to mount root filesystem");
        }
        if (verbose) {
            char buf[256];
            snprintf(buf, sizeof(buf), "mount failed: %s", strerror(errno));
            log_init_info(buf);
        }
        log_init_job("retrying root mount...");
        sleep(1);
    }

    log_init_job("switching root");
    umount("/proc");
    umount("/sys");
    umount("/dev");
    umount("/run");

    switch_root();

    log_init_job(std::string("exec ").append(init_path).c_str());

    char *argv[] = {init_path, nullptr};
    char *envp[] = {
        (char*)"HOME=/",
        (char*)"TERM=linux",
        (char*)"PATH=/sbin:/bin:/usr/sbin:/usr/bin",
        nullptr
    };
    execve(init_path, argv, envp);

    {
        char buf[256];
        snprintf(buf, sizeof(buf), "execve failed: %s", strerror(errno));
        log_init_err(buf);
    }
    panic("failed to execute init");

    return 1;
}
