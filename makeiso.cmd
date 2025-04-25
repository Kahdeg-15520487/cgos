@echo off
set CURRENT_DIR=%~dp0
REM Remove trailing backslash from path
set CURRENT_DIR=%CURRENT_DIR:~0,-1%
docker run -v %CURRENT_DIR%:/src -v %CURRENT_DIR%:/output cgos-builder