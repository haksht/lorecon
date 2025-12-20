@echo off
REM ESP32 LoRa Sniffer v2.2.0 Flash Script for Windows
REM Requires: pip install esptool

setlocal

REM Auto-detect COM port or use argument
if "%1"=="" (
    echo Detecting COM port...
    for /f "tokens=1" %%i in ('mode ^| findstr "COM"') do set PORT=%%i
    if not defined PORT (
        echo ERROR: No COM port detected. Please specify port: flash.bat COM3
        exit /b 1
    )
) else (
    set PORT=%1
)

echo.
echo ============================================
echo ESP32 LoRa Sniffer v2.2.0 Flasher
echo ============================================
echo Target port: %PORT%
echo.
echo Flashing complete firmware image...
echo (This takes about 90 seconds)
echo.

esptool.py --chip esp32s3 --port %PORT% --baud 921600 write_flash 0x0 esp32-lora-sniffer-v2.2.0-full.bin

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Flash FAILED. Try:
    echo   1. Hold BOOT button while plugging in USB
    echo   2. Run: flash.bat COM3  (specify your port)
    echo   3. Run: pip install esptool
    exit /b 1
)

echo.
echo ============================================
echo SUCCESS! Device flashed.
echo ============================================
echo.
echo Next steps:
echo   1. Unplug and replug the device
echo   2. Connect to WiFi: ESP32-LoRa-Sniffer-XXYYZZ
echo   3. Password: recon-XXYYZZ (matches SSID suffix)
echo   4. Browse to: http://192.168.4.1
echo.
