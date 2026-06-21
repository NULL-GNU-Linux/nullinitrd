# `nullinitrd`

An initramfs generator used in [NULL GNU/Linux](https://null.obsidianos.xyz) and [Redrose GNU/Linux](https://redroselinux.org/) written in C++.

#### Known issues
- nullinitrd is broken with NVMe drives
- currently no fully stable version; 1.1 will be released soon

## Build from source

Clone the repo:

```bash
git clone https://github.com/NULL-GNU-Linux/nullinitrd --depth 1
cd nullinitrd
```

Build:

```bash
make defconfig
# make menuconfig
make
```

Install:

```bash
sudo make install
```

As always, the `DESTDIR` and `PREFIX` enviroment variables are supported.

## Quick test

We have a script to create a bootable system with nullinitrd.

```bash
sh tests/test_system.sh path-to-kernel
```

## Configuration and usage

## Rebuild initramfs

Simply run:

```bash
nullinitrd
```

## Configuration file

The `config.default` file has the file written to the configuration file
during install. 

| Option | Default | Description |
|--------|---------|-------------|
| `COMPRESSION` | `zstd` | Compression algorithm to use. Supported: `zstd`, `gzip`, `xz`, `lz4`, `bzip2`, `lzma`, `none` |
| `ROOTFS_TYPE` | `ext4` | Filesystem type to use when mounting root. |
| `INIT_PATH` | `/sbin/init` | Path to the init binary (NOT the initramfs init.) |
| `AUTODETECT_MODULES` | `n` | Automatically detect and include kernel modules. |
| `MODULES` | empty | Additional kernel modules to include (space-separated). |
| `HOOKS` | `keyboard` | Hooks to run during initramfs creation. |
| `FEATURE_LVM` | `n` | Include LVM support. |
| `FEATURE_LUKS` | `n` | Include LUKS support. |
| `FEATURE_MDADM` | `n` | Include MDADM support. |
| `FEATURE_BTRFS` | `n` | Include Btrfs support. |
| `FEATURE_ZFS` | `n` | Include ZFS support. |

### Kernel Parameters

| Parameter | Description |
|-----------|-------------|
| `root=` | Root device (e.g., `/dev/sda1`, `UUID=...`, `PARTUUID=...`, `LABEL=...`) |
| `rootfstype=` | Root filesystem type |
| `rootflags=` | Mount flags for root |
| `rootdelay=` | Seconds to wait before mounting root |
| `init=` | Path to init on root filesystem |
| `rw` | Mount root read-write |
| `ro` | Mount root read-only |
| `rd.debug` | Enable verbose initramfs output |
| `initrd.debug` | Alias for `rd.debug` |
| `rd.modules=` | Additional modules to load (comma-separated) |

## Build Configuration

Run `make menuconfig` or edit `.config`:

| Option | Description |
|--------|-------------|
| `CONFIG_STATIC=y` | Static linking |
| `CONFIG_DEBUG=y` | Debug build |
| `CONFIG_LTO=y` | Link-time optimization |

## Hooks

Custom hooks can be placed in `{/etc,/usr/[local]/share}/nullinitrd/hooks/` and enabled via the `HOOKS` config option.

To create a hook template:

```bash
nullinitrd hook-template my-hook
```

## Packaging

### `.tar.gz`

Run:

```bash
make tgz-pkg
```

Creates a tgz with a dir called `stage` with the files to install and a README.

### [Car](https://github.com/redroselinux/car) `.tar.zst`

Run:

```bash
make car-pkg
```

## Authors

- **[neoapps-dev](https://github.com/neoapps-dev)** \<neo@obsidianos.xyz>
- **[mostypc123](https://github.com/mostypc123)** \<mostypc123@redroselinux.org>
