@echo off
chcp 65001 >nul
cd /d "%~dp0"

set CHIP=esp32c3
set BAUD=921600
set ESPFLAGS=--before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 4MB

set FILES=0x0 "Pocket.ino.bootloader.bin" 0x8000 "Pocket.ino.partitions.bin" 0x10000 "Pocket.ino.bin" 0x100000 "fs.bin"

if exist "boot_app0.bin" (
    set "FILES=%FILES% 0xe000 "boot_app0.bin""
)

echo.
echo ============================================
echo   Pocket - ESP32-C3
echo ============================================

if "%~1"=="" (
    echo   自动扫描串口...
    echo.
    esptool.exe --chip %CHIP% --baud %BAUD% %ESPFLAGS% %FILES%
) else (
    echo   串口: %~1
    echo.
    esptool.exe --chip %CHIP% --port %~1 --baud %BAUD% %ESPFLAGS% %FILES%
)

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================
    echo   烧录失败! 可尝试指定串口: flash.bat COM5
    echo ============================================
    pause
    exit /b 1
)

echo.
echo ============================================
echo   烧录完成! 设备已复位.
echo ============================================
pause
exit /b 0
