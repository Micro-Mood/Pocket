@echo off
chcp 65001 >nul
:: ============================================================
::  build_spiffs.bat — 将 SPIFFS/ 目录打包为 fs.bin 镜像
::  用法: 双击运行，或命令行: build_spiffs.bat
::  输出: fs.bin (位于当前目录)
:: ============================================================

set IMAGE_SIZE=0x300000
set FLASH_ADDR=0x100000
set INPUT=SPIFFS
set OUTPUT=fs.bin

echo [1/1] 正在生成 SPIFFS 镜像...
echo   镜像大小: %IMAGE_SIZE% (3MB)
echo   输入目录: %INPUT%
echo   输出文件: %OUTPUT%
echo.

python spiffs.py %IMAGE_SIZE% %INPUT% %OUTPUT%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [错误] 镜像生成失败，请检查 Python 环境和 SPIFFS/ 目录。
    pause
    exit /b 1
)

echo.
echo [完成] %OUTPUT% 已生成。
echo.
echo 烧录命令:
echo   esptool.exe --chip esp32c3 write_flash %FLASH_ADDR% %OUTPUT%
echo.
pause
