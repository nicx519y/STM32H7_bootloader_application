@echo off
echo STM32H750 调试工具

:menu
echo.
echo 请选择操作:
echo 1. 编译并烧录Bootloader
echo 2. 编译并烧录Application  
echo 3. 完整烧录 (Bootloader + Application)
echo 4. 仅编译Application
echo 5. 仅编译Bootloader
echo 0. 退出

set /p choice=请输入选择 (0-5): 

if "%choice%"=="1" goto flash_bootloader
if "%choice%"=="2" goto flash_application
if "%choice%"=="3" goto full_flash
if "%choice%"=="4" goto build_app
if "%choice%"=="5" goto build_boot
if "%choice%"=="0" goto exit
goto menu

:flash_bootloader
python debug_scripts/debug_setup.py flash-bootloader
pause
goto menu

:flash_application  
python debug_scripts/debug_setup.py flash-application
pause
goto menu

:full_flash
python debug_scripts/debug_setup.py full-flash
pause
goto menu

:build_app
cd application
make
cd ..
pause
goto menu

:build_boot
cd bootloader  
make
cd ..
pause
goto menu

:exit
