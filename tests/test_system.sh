#!/bin/sh
set -e

VMLINUZ="${1:-./vmlinuz}"
[ -f "$VMLINUZ" ] || { echo "Usage: $0 [vmlinuz]"; exit 1; }
VMLINUZ="$(realpath "$VMLINUZ")"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOPLEVEL="$(cd "$SCRIPT_DIR/.." && pwd)"
SYSTEM="$SCRIPT_DIR/test-system"
TESTS_DIR="$SCRIPT_DIR"

WORKDIR="$(mktemp -d /tmp/nullinitrd-test.XXXXXX)"
trap 'rm -rf "$WORKDIR"' EXIT

make -j"$(nproc)" -C "$TOPLEVEL"

echo "Getting busybox from oddstatic"
BUSYBOX="$WORKDIR/busybox"
if command -v busybox >/dev/null 2>&1; then
    cp "$(command -v busybox)" "$BUSYBOX"
else
    curl -Lo "$BUSYBOX" "https://files.obsidianos.xyz/~odd/static/busybox"
fi
chmod +x "$BUSYBOX"

ROOTFS_DIR="$WORKDIR/rootfs"
mkdir -p "$ROOTFS_DIR"/{bin,sbin,etc,dev,proc,sys,tmp}
cp "$BUSYBOX" "$ROOTFS_DIR/bin/busybox"
chmod +x "$ROOTFS_DIR/bin/busybox"
for applet in sh mount umount ls cat ps; do
    ln -sf busybox "$ROOTFS_DIR/bin/$applet"
done
cp "$SYSTEM/init" "$ROOTFS_DIR/sbin/init"
chmod +x "$ROOTFS_DIR/sbin/init"

ROOTFS_IMG="$WORKDIR/rootfs.img"
truncate -s 64M "$ROOTFS_IMG"
mkfs.ext4 -F -d "$ROOTFS_DIR" "$ROOTFS_IMG" >/dev/null 2>&1

cd "$TOPLEVEL"
./bin/nullinitrd \
    -o "$WORKDIR/initrd.img" \
    -k "$(uname -r)" \
    -v \
    -c "$SYSTEM/nullinitrd.conf"
cd "$TESTS_DIR"

ISO_DIR="$WORKDIR/iso"
mkdir -p "$ISO_DIR/boot/grub"
cp "$VMLINUZ" "$ISO_DIR/boot/vmlinuz"
cp "$WORKDIR/initrd.img" "$ISO_DIR/boot/initrd.img"
cp "$SYSTEM/grub.cfg" "$ISO_DIR/boot/grub/grub.cfg"

grub-mkrescue -o nullinitrd-test.iso "$ISO_DIR" 2>/dev/null
cp "$ROOTFS_IMG" rootfs.img

echo "output: $PWD"
qemu-system-x86_64 -cdrom nullinitrd-test.iso -drive file=rootfs.img,format=raw -m 512M
