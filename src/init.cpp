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

static char cmdline[4096];
static char root_dev[256] = "/dev/sda1";
static char root_type[32] = "ext4";
static char root_flags[256] = "ro";
static char init_path[256] = "/sbin/init";
static int root_delay = 0;
static bool verbose = false;

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
    mkdir(tgt, 0755);
    if (mount(src, tgt, type, flags, data) < 0 && errno != EBUSY) {
        ERR(":: mount failed: ");
        write(STDERR_FILENO, tgt, strlen(tgt));
        ERR(" (");
        print_str(strerror(errno));
        ERR(")\n");
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

static int load_module(const char *path) {
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

    return 0;
}

static int module_count = 0;

static void load_modules_from_dir(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) {
        if (verbose) {
            MSG("::   cannot open dir: ");
            print_str(dir);
            MSG("\n");
        }
        return;
    }

    struct dirent *ent;
    char path[512];

    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.') continue;

        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(path, &st) < 0) continue;

        if (S_ISDIR(st.st_mode)) {
            load_modules_from_dir(path);
        } else if (strstr(ent->d_name, ".ko")) {
            if (load_module(path) == 0) {
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

    char moddir[256];
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

    load_modules_from_dir(moddir);

    MSG("::   loaded ");
    print_num(module_count);
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
    do_mount("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC, "gid=5,mode=0620");

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
    umount("/dev/pts");
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