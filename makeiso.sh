#!/bin/bash

# CGOS Build Script - Creates bootable ISO image
set -e  # Exit on any error

# Colors for cleaner output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}=== CGOS Build System ===${NC}"

# Build kernel
echo -e "${BLUE}[1/4]${NC} Building kernel..."
make -s
echo -e "${GREEN}✓${NC} Kernel built successfully"

# Prepare ISO structure
echo -e "${BLUE}[2/4]${NC} Preparing ISO structure..."
mkdir -p iso_root/boot iso_root/boot/limine iso_root/EFI/BOOT

# Copy files quietly
echo -e "${BLUE}[3/4]${NC} Installing bootloader files..."
cp bin/cgos iso_root/boot/
cp limine.conf limine-bin/limine-bios.sys limine-bin/limine-bios-cd.bin \
   limine-bin/limine-uefi-cd.bin iso_root/boot/limine/
cp limine-bin/BOOTX64.EFI iso_root/EFI/BOOT/
cp limine-bin/BOOTIA32.EFI iso_root/EFI/BOOT/
echo -e "${GREEN}✓${NC} Files installed"

# Create ISO and install bootloader
echo -e "${BLUE}[4/4]${NC} Creating bootable ISO..."
xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
        -apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        iso_root -o cgos.iso 2>/dev/null

./limine-bin/limine bios-install cgos.iso 2>/dev/null

echo -e "${GREEN}✓${NC} Build complete: ${YELLOW}cgos.iso${NC} ready"

# # Create an empty zeroed-out 64MiB image file.
# dd if=/dev/zero bs=1M count=0 seek=64 of=image.hdd

# # Create a partition table.
# PATH=$PATH:/usr/sbin:/sbin sgdisk image.hdd -n 1:2048 -t 1:ef00 -m 1

# # Install the Limine BIOS stages onto the image.
# limine bios-install image.hdd

# # Format the image as fat32.
# mformat -i image.hdd@@1M

# # Make relevant subdirectories.
# mmd -i image.hdd@@1M ::/EFI ::/EFI/BOOT ::/boot ::/boot/limine

# # Copy over the relevant files.
# mcopy -i image.hdd@@1M bin/cgos ::/boot
# mcopy -i image.hdd@@1M limine.conf ../limine/limine-bios.sys ::/boot/limine
# mcopy -i image.hdd@@1M ../limine/BOOTX64.EFI ::/EFI/BOOT
# mcopy -i image.hdd@@1M ../limine/BOOTIA32.EFI ::/EFI/BOOT