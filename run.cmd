@if not exist disk.img (
    echo Creating disk.img...
    qemu-img create -f raw disk.img 128M
)

qemu-system-x86_64.exe -M pc -drive file=cgos.iso,format=raw,index=0,media=cdrom -boot d -m 2G -netdev user,id=net0,hostfwd=tcp::8080-:8080 -device e1000,netdev=net0 -drive file=disk.img,format=raw,if=ide,index=1 -debugcon stdio
@REM  -no-shutdown -no-reboot