qemu-system-x86_64 -M q35 -drive file=cgos.iso,format=raw -boot d -m 2G -no-reboot
# qemu-system-x86_64 -M q35 -drive file=image.iso,format=raw -boot d -m 2G -no-reboot -d int
# qemu-system-x86_64 -M q35 -drive file=image.iso,format=raw -boot d -m 2G -s -S