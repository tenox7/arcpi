#!/usr/bin/env bash
# Build a flashable SD image that boots the NT 3.5 ARC loader on a real Raspberry
# Pi 2 with HDMI output. Layout: MBR + a single FAT32 partition holding the Pi GPU
# firmware, config.txt, and our loader.bin (as kernel=loader.bin).
#
# Modeled on /Users/tenox/VM/RPI/make-sd-image.sh (the proven U-Boot recipe), but
# boots our raw loader directly instead of U-Boot. The loader is linked at 0x8000
# (the firmware's native 32-bit load address), so no kernel_address is needed.
#
# Usage:  cd ARM32/build && ./build.sh        # produce ../obj/loader.bin first
#         cd ../sdcard && ./make-sd-image.sh   # -> ../obj/nt-loader-sd.img
# Flash:  sudo dd if=../obj/nt-loader-sd.img of=/dev/rdiskN bs=4m   (diskutil list)
set -euo pipefail

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

LOADER="../obj/loader.bin"
IMG="../obj/nt-loader-sd.img"
FWCACHE="firmware"
SIZE_MB=64

need() { command -v "$1" >/dev/null 2>&1 || { echo "missing required tool: $1"; exit 1; }; }
need docker
[[ -f "$LOADER" ]] || { echo "missing $LOADER - build it first: (cd ../build && ./build.sh)"; exit 1; }

# Cache the Pi firmware locally so repeated builds do not re-download.
mkdir -p "$FWCACHE"
base="https://raw.githubusercontent.com/raspberrypi/firmware/stable/boot"
for f in bootcode.bin start.elf fixup.dat bcm2709-rpi-2-b.dtb; do
  [[ -s "$FWCACHE/$f" ]] || { echo ">> fetching $f"; curl -fsSL -o "$FWCACHE/$f" "$base/$f"; }
done

echo ">> assembling $IMG (firmware + config.txt + loader.bin, MBR + FAT32) in Docker"
# Mount the ARM32 root so ../obj/loader.bin is visible inside the container; run
# from /work/sdcard so the relative paths below match the host layout.
ARM32ROOT="$(cd .. && pwd)"
docker run --rm -v "$ARM32ROOT":/work -w /work/sdcard \
  -e IMG="$IMG" -e SIZE_MB="$SIZE_MB" debian:bookworm bash -c '
set -e
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq dosfstools mtools fdisk >/dev/null

rm -rf /tmp/sd && mkdir -p /tmp/sd
cp firmware/bootcode.bin firmware/start.elf firmware/fixup.dat firmware/bcm2709-rpi-2-b.dtb /tmp/sd/
cp config.txt /tmp/sd/config.txt
cp ../obj/loader.bin /tmp/sd/loader.bin

dd if=/dev/zero of="$IMG" bs=1M count="$SIZE_MB" status=none
printf "label: dos\nstart=2048, type=c, bootable\n" | sfdisk "$IMG" >/dev/null
psect=$(( SIZE_MB*2048 - 2048 ))
dd if=/dev/zero of=/tmp/part.img bs=512 count="$psect" status=none
mkfs.vfat -F 32 -n NTBOOT /tmp/part.img >/dev/null
mcopy -i /tmp/part.img /tmp/sd/* ::
dd if=/tmp/part.img of="$IMG" bs=512 seek=2048 conv=notrunc status=none

echo "--- partition table ---"; sfdisk -l "$IMG"
echo "--- FAT contents ---"; mdir -i /tmp/part.img ::
'
echo ">> built $IMG ($(du -h "$IMG" | cut -f1))."
echo "   Flash with Raspberry Pi Imager / balenaEtcher, or:"
echo "     sudo dd if=$IMG of=/dev/rdiskN bs=4m    (find N via 'diskutil list')"
echo "   Then: SD into a Pi 2, connect HDMI (+ USB-serial on GPIO14/15 for the log), power on."
