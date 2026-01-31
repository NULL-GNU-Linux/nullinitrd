#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <linux/reboot.h>
#define MSG(x) write(STDOUT_FILENO, x, sizeof(x) - 1)
#define ERR(x) write(STDERR_FILENO, x, sizeof(x) - 1)
#define MAX_MODULES 512
#define MAX_DEPS 32
#define MAX_PATH_LEN 512
#define MAX_NAME_LEN 128
static char cmdline[4096];
static char root_dev[256] = "/dev/sda1";
static char root_type[32] = "ext4";
static char root_flags[256] = "ro";
static char init_path[256] = "/sbin/init";
static int root_delay = 0;
static bool verbose = false;
struct ModuleEntry {
    char name[MAX_NAME_LEN];
    char path[MAX_PATH_LEN];
    char deps[MAX_DEPS][MAX_NAME_LEN];
    int dep_count;
    bool loaded;
};

static ModuleEntry modules[MAX_MODULES];
static int module_count = 0;
static char moddir[256];
static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

static void print_num(int n) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n);
    print_str(buf);
}

static void panic(const char *msg) {
    ERR(":: PANIC: ");
    write(STDERR_FILENO, msg, strlen(msg));
    ERR("\n:: dropping to infinite sleep\n");
    for (;;) sleep(3600);
}

static void do_mount(const char *src, const char *tgt, const char *type, unsigned long flags, const void *data) {
    char parent[PATH_MAX];
    strncpy(parent, tgt, sizeof(parent));
    parent[sizeof(parent)-1] = '\0';
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        mkdir(parent, 0755);
    }

    mkdir(tgt, 0755);
    if (mount(src, tgt, type, flags, data) < 0 && errno != EBUSY) {
        ERR(":: mount failed\n");
        ERR("   source: "); write(STDERR_FILENO, src, strlen(src)); ERR("\n");
        ERR("   target: "); write(STDERR_FILENO, tgt, strlen(tgt)); ERR("\n");
        ERR("   type  : "); write(STDERR_FILENO, type ? type : "(null)", type ? strlen(type) : 6); ERR("\n");
        ERR("   flags : "); char buf[32]; snprintf(buf, sizeof(buf), "%#lx", flags); ERR(buf); ERR("\n");
        ERR("   errno : "); char errbuf[64]; snprintf(errbuf, sizeof(errbuf), "%d (%s)\n", errno, strerror(errno)); ERR(errbuf);
    }
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
        MSG(":: cmdline: ");
        print_str(cmdline);
        MSG("\n");
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
        }
    }
}

static int load_module_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        if (verbose) {
            MSG("::   open failed: ");
            print_str(path);
            MSG(" (");
            print_str(strerror(errno));
            MSG(")\n");
        }
        return -1;
    }

    int ret = syscall(SYS_finit_module, fd, "", 0);
    int err = errno;
    close(fd);

    if (ret < 0 && err != EEXIST) {
        if (verbose) {
            MSG("::   finit_module failed: ");
            print_str(path);
            MSG(" (");
            print_str(strerror(err));
            MSG(")\n");
        }
        return -1;
    }

    if (verbose && ret == 0) {
        MSG("::   loaded: ");
        print_str(path);
        MSG("\n");
    }

    return (ret == 0 || err == EEXIST) ? 0 : -1;
}

static void strip_compression_ext(char *path) {
    size_t len = strlen(path);
    if (len > 4 && strcmp(path + len - 4, ".zst") == 0) {
        path[len - 4] = '\0';
    } else if (len > 3 && strcmp(path + len - 3, ".xz") == 0) {
        path[len - 3] = '\0';
    } else if (len > 3 && strcmp(path + len - 3, ".gz") == 0) {
        path[len - 3] = '\0';
    }
}

static void path_to_name(const char *path, char *name, size_t name_size) {
    const char *base = strrchr(path, '/');
    if (base) {
        base++;
    } else {
        base = path;
    }

    strncpy(name, base, name_size - 1);
    name[name_size - 1] = '\0';
    char *ext = strstr(name, ".ko");
    if (ext) {
        *ext = '\0';
    }

    for (char *p = name; *p; p++) {
        if (*p == '-') *p = '_';
    }
}

static int find_module_by_name(const char *name) {
    char normalized[MAX_NAME_LEN];
    strncpy(normalized, name, sizeof(normalized) - 1);
    normalized[sizeof(normalized) - 1] = '\0';
    for (char *p = normalized; *p; p++) {
        if (*p == '-') *p = '_';
    }

    for (int i = 0; i < module_count; i++) {
        if (strcmp(modules[i].name, normalized) == 0) {
            return i;
        }
    }
    return -1;
}

static int load_module_with_deps(int idx) {
    if (idx < 0 || idx >= module_count) return -1;
    if (modules[idx].loaded) return 0;
    for (int i = 0; i < modules[idx].dep_count; i++) {
        int dep_idx = find_module_by_name(modules[idx].deps[i]);
        if (dep_idx >= 0) {
            if (load_module_with_deps(dep_idx) < 0) {
                if (verbose) {
                    MSG("::   warning: failed to load dependency ");
                    print_str(modules[idx].deps[i]);
                    MSG(" for ");
                    print_str(modules[idx].name);
                    MSG("\n");
                }
            }
        } else if (verbose) {
            MSG("::   warning: dependency not found: ");
            print_str(modules[idx].deps[i]);
            MSG("\n");
        }
    }

    char full_path[MAX_PATH_LEN];
    snprintf(full_path, sizeof(full_path), "%s/%s", moddir, modules[idx].path);
    if (load_module_file(full_path) == 0) {
        modules[idx].loaded = true;
        return 0;
    }

    return -1;
}

static void parse_modules_dep() {
    char dep_path[MAX_PATH_LEN];
    snprintf(dep_path, sizeof(dep_path), "%s/modules.dep", moddir);
    int fd = open(dep_path, O_RDONLY);
    if (fd < 0) {
        if (verbose) {
            MSG("::   modules.dep not found, will load without dependency info\n");
        }
        return;
    }

    static char dep_buf[65536];
    ssize_t total = 0;
    ssize_t n;
    while ((n = read(fd, dep_buf + total, sizeof(dep_buf) - total - 1)) > 0) {
        total += n;
        if (total >= (ssize_t)(sizeof(dep_buf) - 1)) break;
    }
    close(fd);
    dep_buf[total] = '\0';

    if (verbose) {
        MSG("::   parsing modules.dep (");
        print_num((int)total);
        MSG(" bytes)\n");
    }

    char *line = dep_buf;
    while (*line && module_count < MAX_MODULES) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        if (*line) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                ModuleEntry *entry = &modules[module_count];
                strncpy(entry->path, line, MAX_PATH_LEN - 1);
                entry->path[MAX_PATH_LEN - 1] = '\0';
                strip_compression_ext(entry->path);
                path_to_name(entry->path, entry->name, MAX_NAME_LEN);
                entry->dep_count = 0;
                entry->loaded = false;
                char *deps = colon + 1;
                while (*deps == ' ') deps++;
                while (*deps && entry->dep_count < MAX_DEPS) {
                    char *space = strchr(deps, ' ');
                    if (space) *space = '\0';

                    if (*deps) {
                        path_to_name(deps, entry->deps[entry->dep_count], MAX_NAME_LEN);
                        entry->dep_count++;
                    }

                    if (space) {
                        deps = space + 1;
                        while (*deps == ' ') deps++;
                    } else {
                        break;
                    }
                }

                module_count++;
            }
        }

        if (newline) {
            line = newline + 1;
        } else {
            break;
        }
    }

    if (verbose) {
        MSG("::   found ");
        print_num(module_count);
        MSG(" modules in modules.dep\n");
    }
}

static void scan_modules_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    char path[MAX_PATH_LEN];
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        struct stat st;
        if (stat(path, &st) < 0) continue;
        if (S_ISDIR(st.st_mode)) {
            scan_modules_dir(path);
        } else if (strstr(ent->d_name, ".ko") && module_count < MAX_MODULES) {
            char name[MAX_NAME_LEN];
            path_to_name(ent->d_name, name, MAX_NAME_LEN);
            if (find_module_by_name(name) < 0) {
                ModuleEntry *entry = &modules[module_count];
                strncpy(entry->name, name, MAX_NAME_LEN - 1);
                const char *rel_path = path + strlen(moddir) + 1;
                strncpy(entry->path, rel_path, MAX_PATH_LEN - 1);
                entry->dep_count = 0;
                entry->loaded = false;
                module_count++;
            }
        }
    }
    closedir(d);
}

static void load_modules() {
    MSG(":: loading modules\n");

    struct utsname uts;
    if (uname(&uts) < 0) {
        ERR(":: uname failed\n");
        return;
    }

    snprintf(moddir, sizeof(moddir), "/usr/lib/modules/%s", uts.release);

    if (verbose) {
        MSG("::   module dir: ");
        print_str(moddir);
        MSG("\n");
    }

    struct stat st;
    if (stat(moddir, &st) < 0) {
        MSG("::   module dir not found, skipping\n");
        return;
    }

    parse_modules_dep();
    scan_modules_dir(moddir);
    int loaded_count = 0;
    for (int i = 0; i < module_count; i++) {
        if (!modules[i].loaded) {
            if (load_module_with_deps(i) == 0) {
                loaded_count++;
            }
        }
    }

    MSG("::   loaded ");
    print_num(loaded_count);
    MSG(" modules\n");
}

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
            MSG("::   resolving: ");
            print_str(link);
            MSG("\n");
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
            MSG(":: waiting for root device...\n");
            sleep(1);
        }
        ERR(":: failed to resolve device\n");
    }

    return dev;
}

static void switch_root() {
    chdir("/mnt/root");
    mount(".", "/", nullptr, MS_MOVE, nullptr);
    chroot(".");
    chdir("/");
}

int main() {
    MSG(":: nullinitrd\n");
    do_mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr);
    do_mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr);
    do_mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, "mode=0755");
    do_mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755");
    mkdir("/dev/pts", 0755);
    parse_cmdline();
    load_modules();
    if (root_delay > 0) {
        MSG(":: waiting ");
        print_num(root_delay);
        MSG("s for root device\n");
        sleep(root_delay);
    }

    char *dev = resolve_device(root_dev);
    MSG(":: mounting root: ");
    print_str(dev);
    MSG(" (");
    print_str(root_type);
    MSG(")\n");
    mkdir("/mnt/root", 0755);
    unsigned long mflags = 0;
    if (strstr(root_flags, "ro")) mflags |= MS_RDONLY;
    for (int i = 0; i < 30; i++) {
        if (mount(dev, "/mnt/root", root_type, mflags, nullptr) == 0) break;
        if (i == 29) {
            ERR(":: mount error: ");
            print_str(strerror(errno));
            ERR("\n");
            panic("failed to mount root filesystem");
        }
        if (verbose) {
            MSG("::   mount failed: ");
            print_str(strerror(errno));
            MSG("\n");
        }
        MSG(":: retrying root mount...\n");
        sleep(1);
    }

    MSG(":: switching root\n");
    umount("/proc");
    umount("/sys");
    umount("/dev");
    umount("/run");
    switch_root();
    MSG(":: exec ");
    print_str(init_path);
    MSG("\n");
    char *argv[] = {init_path, nullptr};
    char *envp[] = {(char*)"HOME=/", (char*)"TERM=linux", (char*)"PATH=/sbin:/bin:/usr/sbin:/usr/bin", nullptr};
    execve(init_path, argv, envp);
    ERR(":: execve failed: ");
    print_str(strerror(errno));
    ERR("\n");
    panic("failed to execute init");
    return 1;
}
