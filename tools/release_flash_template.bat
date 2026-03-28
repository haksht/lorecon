@echo off
REM ESP32 LoRa Sniffer - Flash Script
REM Works from CMD or PowerShell.
REM
REM Usage: flash.bat <board> [port]
REM
REM Boards: heltec_v3 | t3_s3 | tbeam_supreme
REM
REM Examples:
REM   flash.bat heltec_v3
REM   flash.bat heltec_v3 COM3
REM   flash.bat t3_s3 COM9
REM   flash.bat tbeam_supreme COM11
REM
REM Requires: pip install esptool

setlocal

set BOARD=%1
set PORT=%2

if "%BOARD%"=="heltec_v3" (
    set LABEL=Heltec WiFi LoRa 32 V3/V4
    set FLASH_SIZE=8MB
    set PORT_HINT=Usually COM3 - CP210x shown in Device Manager
) else if "%BOARD%"=="t3_s3" (
    set LABEL=LilyGO T3-S3 V1.2/V1.3
    set FLASH_SIZE=4MB
    set PORT_HINT=Usually COM9 - native USB, hold BOOT button if needed
) else if "%BOARD%"=="tbeam_supreme" (
    set LABEL=LilyGO T-Beam Supreme
    set FLASH_SIZE=8MB
    set PORT_HINT=Usually COM11 - native USB, hold BOOT button if needed
) else (
    echo ERROR: Unknown board '%BOARD%'
    echo.
    echo Usage: flash.bat ^<board^> [port]
    echo Boards:
    echo   heltec_v3      - Heltec WiFi LoRa 32 V3/V4
    echo   t3_s3          - LilyGO T3-S3 V1.2/V1.3
    echo   tbeam_supreme  - LilyGO T-Beam Supreme
    exit /b 1
)

set BIN=%~dp0%BOARD%\full.bin

if not exist "%BIN%" (
    echo ERROR: Binary not found: %BIN%
    echo Make sure you extracted the full release zip.
    exit /b 1
)

REM Auto-detect port if not specified
if "%PORT%"=="" (
    echo Detecting COM port...
    for /f "tokens=1 delims=:" %%i in ('mode ^| findstr /i "COM"') do (
        set PORT=%%i
        goto :found_port
    )
    echo ERROR: No COM port detected.
    echo Hint: %PORT_HINT%
    echo Try: flash.bat %BOARD% COM3
    exit /b 1
    :found_port
    set PORT=%PORT: =%
)

echo.
echo ============================================
echo   ESP32 LoRa Sniffer Flasher
echo   Board: %LABEL%
echo   Port:  %PORT%
echo ============================================
echo.
echo Flashing full image - this takes 30-90 seconds...
echo.

python -m esptool --chip esp32s3 --port %PORT% --baud 921600 write-flash --flash-size %FLASH_SIZE% 0x0 "%BIN%"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Flash FAILED. Try:
    echo   1. Hold BOOT button while plugging in USB, then run again
    echo   2. Specify port: flash.bat %BOARD% COM3
    echo   3. Lower baud: edit this script, change 921600 to 115200
    echo   4. Install esptool: pip install esptool
    exit /b 1
)

echo.
echo ============================================
echo   SUCCESS! Device flashed.
echo ============================================
echo.
echo Next steps:
echo   1. Power-cycle the device - unplug and replug USB
echo   2. Connect to WiFi AP:  LoRa-XXYYZZ
echo   3. Password:            recon-XXYYZZ  (matches SSID suffix)
echo   4. Open browser:        http://192.168.4.1
echo.
