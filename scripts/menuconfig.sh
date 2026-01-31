#!/bin/sh
CONFIG_FILE="$PWD/.config"
[ -f "$CONFIG_FILE" ] || cp config.defconfig "$CONFIG_FILE"
. "$CONFIG_FILE"

bool_to_status() { [ "$1" = "y" ] && echo "on" || echo "off"; }
SELECTED=$(dialog --title "nullinitrd Configuration" \
    --checklist "Select options:" 16 50 8 \
    "STATIC" "Static linking" "$(bool_to_status "$CONFIG_STATIC")" \
    "DEBUG" "Debug build" "$(bool_to_status "$CONFIG_DEBUG")" \
    "LTO" "Link-time optimization" "$(bool_to_status "$CONFIG_LTO")" \
    "LVM" "LVM support" "$(bool_to_status "$CONFIG_FEATURE_LVM")" \
    "LUKS" "LUKS encryption" "$(bool_to_status "$CONFIG_FEATURE_LUKS")" \
    "MDADM" "Software RAID" "$(bool_to_status "$CONFIG_FEATURE_MDADM")" \
    "BTRFS" "Btrfs support" "$(bool_to_status "$CONFIG_FEATURE_BTRFS")" \
    "ZFS" "ZFS support" "$(bool_to_status "$CONFIG_FEATURE_ZFS")" \
    3>&1 1>&2 2>&3) || exit 0

CONFIG_STATIC=n CONFIG_DEBUG=n CONFIG_LTO=n
CONFIG_FEATURE_LVM=n CONFIG_FEATURE_LUKS=n CONFIG_FEATURE_MDADM=n
CONFIG_FEATURE_BTRFS=n CONFIG_FEATURE_ZFS=n
for item in $SELECTED; do
    case $(echo "$item" | tr -d '"') in
        STATIC) CONFIG_STATIC=y ;;
        DEBUG) CONFIG_DEBUG=y ;;
        LTO) CONFIG_LTO=y ;;
        LVM) CONFIG_FEATURE_LVM=y ;;
        LUKS) CONFIG_FEATURE_LUKS=y ;;
        MDADM) CONFIG_FEATURE_MDADM=y ;;
        BTRFS) CONFIG_FEATURE_BTRFS=y ;;
        ZFS) CONFIG_FEATURE_ZFS=y ;;
    esac
done

cat > "$CONFIG_FILE" <<EOF
CONFIG_STATIC=$CONFIG_STATIC
CONFIG_DEBUG=$CONFIG_DEBUG
CONFIG_LTO=$CONFIG_LTO
CONFIG_FEATURE_LVM=$CONFIG_FEATURE_LVM
CONFIG_FEATURE_LUKS=$CONFIG_FEATURE_LUKS
CONFIG_FEATURE_MDADM=$CONFIG_FEATURE_MDADM
CONFIG_FEATURE_BTRFS=$CONFIG_FEATURE_BTRFS
CONFIG_FEATURE_ZFS=$CONFIG_FEATURE_ZFS
EOF

dialog --title "Saved" --msgbox "Run 'make clean && make' to rebuild." 6 40
