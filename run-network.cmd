@echo off
echo Available network configurations:
echo 1. User mode networking (NAT) - Default
echo 2. Bridge networking (requires admin rights)
echo 3. TAP networking (advanced)
echo 4. No networking
echo.

set /p choice="Select network option (1-4): "

if "%choice%"=="1" goto usermode
if "%choice%"=="2" goto bridge
if "%choice%"=="3" goto tap
if "%choice%"=="4" goto nonet
goto usermode

:usermode
echo Starting with user mode networking (NAT)...
qemu-system-x86_64.exe -M q35 -drive file=cgos.iso,format=raw -boot d -m 2G -no-reboot ^
    -netdev user,id=net0,hostfwd=tcp::8080-:8080,hostfwd=tcp::2222-:22 ^
    -device e1000,netdev=net0
goto end

:bridge
echo Starting with bridge networking...
qemu-system-x86_64.exe -M q35 -drive file=cgos.iso,format=raw -boot d -m 2G -no-reboot ^
    -netdev bridge,id=net0 ^
    -device e1000,netdev=net0
goto end

:tap
echo Starting with TAP networking...
qemu-system-x86_64.exe -M q35 -drive file=cgos.iso,format=raw -boot d -m 2G -no-reboot ^
    -netdev tap,id=net0,ifname=tap0,script=no,downscript=no ^
    -device e1000,netdev=net0
goto end

:nonet
echo Starting without networking...
qemu-system-x86_64.exe -M q35 -drive file=cgos.iso,format=raw -boot d -m 2G -no-reboot
goto end

:end
