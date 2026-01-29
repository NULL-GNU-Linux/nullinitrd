# nullinitrd

NULL's Modular Initramfs Generator.

## Building

```sh
make defconfig
make
```

## Installation

```sh
sudo make install
```

## Usage

```sh
nullinitrd
```

### Options

| Option | Description |
|--------|-------------|
| `-o, --output FILE` | Output initramfs file (default: `/boot/initrd-<kernel_version>.img`) |
| `-c, --config FILE` | Configuration file (default: `/etc/nullinitrd/config`) |
| `-k, --kernel VER` | Kernel version (default: current) |
| `-v, --verbose` | Verbose output |
| `-h, --help` | Show help |
| `--version` | Show version |

## Configuration

Edit `/etc/nullinitrd/config`:

```sh
COMPRESSION=zstd
ROOTFS_TYPE=ext4
INIT_PATH=/sbin/init
AUTODETECT_MODULES=y
MODULES=
HOOKS=
FEATURE_LVM=n
FEATURE_LUKS=n
FEATURE_MDADM=n
FEATURE_BTRFS=n
FEATURE_ZFS=n
```

### Compression

Supported: `zstd`, `gzip`, `xz`, `lz4`, `bzip2`, `lzma`, `none`

### Kernel Parameters

| Parameter | Description |
|-----------|-------------|
| `root=` | Root device (e.g., `/dev/sda1`, `UUID=...`, `PARTUUID=...`) |
| `rootfstype=` | Root filesystem type |
| `rootflags=` | Mount flags for root |
| `rootdelay=` | Seconds to wait before mounting root |
| `init=` | Path to init on root filesystem |
| `rw` | Mount root read-write |
| `ro` | Mount root read-only |

## Build Configuration

Run `make menuconfig` or edit `.config`:

| Option | Description |
|--------|-------------|
| `CONFIG_STATIC=y` | Static linking |
| `CONFIG_DEBUG=y` | Debug build |
| `CONFIG_LTO=y` | Link-time optimization |
| `CONFIG_FEATURE_LVM=y` | Include LVM tools |
| `CONFIG_FEATURE_LUKS=y` | Include cryptsetup |
| `CONFIG_FEATURE_MDADM=y` | Include mdadm |
| `CONFIG_FEATURE_BTRFS=y` | Include btrfs modules |
| `CONFIG_FEATURE_ZFS=y` | Include ZFS modules |
