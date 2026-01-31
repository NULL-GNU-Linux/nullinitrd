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
| `-o, --output FILE` | Output initramfs file (default: `/boot/initrd.img`) |
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

## Default Modules

The following modules are loaded by default (if available):

- **NVMe**: `nvme`, `nvme_core`
- **SATA/SCSI**: `ahci`, `sd_mod`, `sr_mod`
- **Filesystems**: `ext4`, `btrfs`, `xfs`, `vfat`, `fat`
- **USB**: `usb_storage`, `uas`, `ehci_hcd`, `ehci_pci`, `xhci_hcd`, `xhci_pci`, `ohci_hcd`, `ohci_pci`
- **Device Mapper**: `dm_mod`, `dm_crypt`
- **RAID**: `raid0`, `raid1`, `raid456`, `md_mod`

Additional modules can be specified via the `MODULES` config option or the `rd.modules=` kernel parameter.

## Features

Enable features in the config file to include additional tools:

| Feature | Description |
|---------|-------------|
| `FEATURE_LVM=y` | Include LVM tools (`lvm`) |
| `FEATURE_LUKS=y` | Include disk encryption (`cryptsetup`) |
| `FEATURE_MDADM=y` | Include software RAID (`mdadm`) |

## Hooks

Custom hooks can be placed in `/usr/share/nullinitrd/hooks/` and enabled via the `HOOKS` config option.

## Dependencies

`nullinitrd` currently only depends on `kmod`.
