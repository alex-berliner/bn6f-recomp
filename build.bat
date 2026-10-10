@echo off
rem Build the recomp ROM on Windows: runs build.sh under WSL (recommended) or a bash with the
rem same tools (MSYS2 / Git Bash with make, gcc, python3, libpng, bison, flex, texinfo, arm-none-eabi-gcc).
rem   build.bat            -^> cport\build\bn6f_lifted.gba
rem   build.bat supercard  -^> also roms\out\bn6f_falzar_cport_supercard.gba
cd /d "%~dp0"
where wsl >nul 2>nul
if %errorlevel%==0 (
    wsl bash ./build.sh %*
    exit /b %errorlevel%
)
where bash >nul 2>nul
if %errorlevel%==0 (
    bash ./build.sh %*
    exit /b %errorlevel%
)
echo Neither WSL nor bash was found. Install WSL (in an admin prompt: wsl --install), then run this again.
exit /b 1
