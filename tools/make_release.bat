@echo off
REM tools\make_release.bat — Build and package release binaries for all boards (Windows CMD)
REM
REM Usage:   make_release.bat <version>
REM Example: make_release.bat v2.3.0
REM
REM Outputs: releases\<version>\   (per-board subdirs + flash scripts)
REM          releases\esp32-lora-sniffer-<version>-binaries.zip
REM
REM Prerequisites: pip install esptool

setlocal enabledelayedexpansion

set VERSION=%1
if "%VERSION%"=="" (
    echo Usage: make_release.bat ^<version^>   e.g. make_release.bat v2.3.0
    exit /b 1
)

set REPO_ROOT=%~dp0..
set RELEASE_DIR=%REPO_ROOT%\releases\%VERSION%
set PIO_BASE=%USERPROFILE%\.platformio
set PIO=%PIO_BASE%\penv\Scripts\pio.exe
set PIO_PYTHON=%PIO_BASE%\penv\Scripts\python.exe
set PIO_ESPTOOL=%PIO_BASE%\packages\tool-esptoolpy\esptool.py

if not exist "%PIO%" (
    set PIO=pio
)

echo ========================================
echo   ESP32 LoRa Sniffer Release Builder
echo   Version: %VERSION%
echo ========================================
echo.

REM ---- Build firmware --------------------------------------------------------
echo [1/3] Building firmware...
"%PIO%" run -e heltec_v3 -e heltec_v4 -e t3_s3 -e tbeam_supreme -d "%REPO_ROOT%"
if %ERRORLEVEL% NEQ 0 ( echo Build failed. & exit /b 1 )

echo.
echo [2/3] Building filesystems...
"%PIO%" run -t buildfs -e heltec_v3 -e heltec_v4 -e t3_s3 -e tbeam_supreme -d "%REPO_ROOT%"
if %ERRORLEVEL% NEQ 0 ( echo Filesystem build failed. & exit /b 1 )

REM ---- Package ---------------------------------------------------------------
echo.
echo [3/3] Packaging...

if exist "%RELEASE_DIR%" (
    echo   WARNING: %RELEASE_DIR% already exists - overwriting.
    rmdir /s /q "%RELEASE_DIR%"
)
mkdir "%RELEASE_DIR%"

REM --- heltec_v3 ---
echo.
echo   [heltec_v3] Heltec WiFi LoRa 32 V3
mkdir "%RELEASE_DIR%\heltec_v3"
copy "%REPO_ROOT%\.pio\build\heltec_v3\bootloader.bin" "%RELEASE_DIR%\heltec_v3\" >nul
copy "%REPO_ROOT%\.pio\build\heltec_v3\partitions.bin"  "%RELEASE_DIR%\heltec_v3\" >nul
copy "%REPO_ROOT%\.pio\build\heltec_v3\firmware.bin"    "%RELEASE_DIR%\heltec_v3\" >nul
copy "%REPO_ROOT%\.pio\build\heltec_v3\littlefs.bin"    "%RELEASE_DIR%\heltec_v3\" >nul
"%PIO_PYTHON%" "%PIO_ESPTOOL%" --chip esp32s3 merge_bin --flash_size 8MB ^
    -o "%RELEASE_DIR%\heltec_v3\full.bin" ^
    0x0      "%RELEASE_DIR%\heltec_v3\bootloader.bin" ^
    0x8000   "%RELEASE_DIR%\heltec_v3\partitions.bin" ^
    0x10000  "%RELEASE_DIR%\heltec_v3\firmware.bin" ^
    0x670000 "%RELEASE_DIR%\heltec_v3\littlefs.bin"
if %ERRORLEVEL% NEQ 0 ( echo merge_bin failed for heltec_v3. & exit /b 1 )

REM --- heltec_v4 ---
echo.
echo   [heltec_v4] Heltec WiFi LoRa 32 V4 (GPS)
mkdir "%RELEASE_DIR%\heltec_v4"
copy "%REPO_ROOT%\.pio\build\heltec_v4\bootloader.bin" "%RELEASE_DIR%\heltec_v4\" >nul
copy "%REPO_ROOT%\.pio\build\heltec_v4\partitions.bin"  "%RELEASE_DIR%\heltec_v4\" >nul
copy "%REPO_ROOT%\.pio\build\heltec_v4\firmware.bin"    "%RELEASE_DIR%\heltec_v4\" >nul
copy "%REPO_ROOT%\.pio\build\heltec_v4\littlefs.bin"    "%RELEASE_DIR%\heltec_v4\" >nul
"%PIO_PYTHON%" "%PIO_ESPTOOL%" --chip esp32s3 merge_bin --flash_size 8MB ^
    -o "%RELEASE_DIR%\heltec_v4\full.bin" ^
    0x0      "%RELEASE_DIR%\heltec_v4\bootloader.bin" ^
    0x8000   "%RELEASE_DIR%\heltec_v4\partitions.bin" ^
    0x10000  "%RELEASE_DIR%\heltec_v4\firmware.bin" ^
    0x670000 "%RELEASE_DIR%\heltec_v4\littlefs.bin"
if %ERRORLEVEL% NEQ 0 ( echo merge_bin failed for heltec_v4. & exit /b 1 )

REM --- t3_s3 ---
echo.
echo   [t3_s3] LilyGO T3-S3 V1.2/V1.3
mkdir "%RELEASE_DIR%\t3_s3"
copy "%REPO_ROOT%\.pio\build\t3_s3\bootloader.bin" "%RELEASE_DIR%\t3_s3\" >nul
copy "%REPO_ROOT%\.pio\build\t3_s3\partitions.bin"  "%RELEASE_DIR%\t3_s3\" >nul
copy "%REPO_ROOT%\.pio\build\t3_s3\firmware.bin"    "%RELEASE_DIR%\t3_s3\" >nul
copy "%REPO_ROOT%\.pio\build\t3_s3\littlefs.bin"    "%RELEASE_DIR%\t3_s3\" >nul
"%PIO_PYTHON%" "%PIO_ESPTOOL%" --chip esp32s3 merge_bin --flash_size 4MB ^
    -o "%RELEASE_DIR%\t3_s3\full.bin" ^
    0x0      "%RELEASE_DIR%\t3_s3\bootloader.bin" ^
    0x8000   "%RELEASE_DIR%\t3_s3\partitions.bin" ^
    0x10000  "%RELEASE_DIR%\t3_s3\firmware.bin" ^
    0x290000 "%RELEASE_DIR%\t3_s3\littlefs.bin"
if %ERRORLEVEL% NEQ 0 ( echo merge_bin failed for t3_s3. & exit /b 1 )

REM --- tbeam_supreme ---
echo.
echo   [tbeam_supreme] LilyGO T-Beam Supreme
mkdir "%RELEASE_DIR%\tbeam_supreme"
copy "%REPO_ROOT%\.pio\build\tbeam_supreme\bootloader.bin" "%RELEASE_DIR%\tbeam_supreme\" >nul
copy "%REPO_ROOT%\.pio\build\tbeam_supreme\partitions.bin"  "%RELEASE_DIR%\tbeam_supreme\" >nul
copy "%REPO_ROOT%\.pio\build\tbeam_supreme\firmware.bin"    "%RELEASE_DIR%\tbeam_supreme\" >nul
copy "%REPO_ROOT%\.pio\build\tbeam_supreme\littlefs.bin"    "%RELEASE_DIR%\tbeam_supreme\" >nul
"%PIO_PYTHON%" "%PIO_ESPTOOL%" --chip esp32s3 merge_bin --flash_size 8MB ^
    -o "%RELEASE_DIR%\tbeam_supreme\full.bin" ^
    0x0      "%RELEASE_DIR%\tbeam_supreme\bootloader.bin" ^
    0x8000   "%RELEASE_DIR%\tbeam_supreme\partitions.bin" ^
    0x10000  "%RELEASE_DIR%\tbeam_supreme\firmware.bin" ^
    0x300000 "%RELEASE_DIR%\tbeam_supreme\littlefs.bin"
if %ERRORLEVEL% NEQ 0 ( echo merge_bin failed for tbeam_supreme. & exit /b 1 )

REM ---- Copy flash scripts from tools -----------------------------------------
copy "%~dp0\release_flash_template.sh"  "%RELEASE_DIR%\flash.sh"  >nul 2>nul || echo (flash.sh not copied - generate from make_release.sh instead)
copy "%~dp0\release_flash_template.bat" "%RELEASE_DIR%\flash.bat" >nul 2>nul || echo (flash.bat not copied - generate from make_release.sh instead)

REM ---- Zip -------------------------------------------------------------------
echo.
echo Creating zip archive...
set ZIP_NAME=esp32-lora-sniffer-%VERSION%-binaries.zip
cd "%REPO_ROOT%\releases"
powershell -Command "Compress-Archive -Path '%VERSION%' -DestinationPath '%ZIP_NAME%' -Force"
if %ERRORLEVEL% NEQ 0 ( echo Zip failed. & exit /b 1 )

echo.
echo ========================================
echo   Release %VERSION% complete!
echo ========================================
echo.
echo   Directory: releases\%VERSION%\
echo   Archive:   releases\%ZIP_NAME%
echo.
echo   Boards packaged:
echo     heltec_v3      Heltec WiFi LoRa 32 V3         (8MB)
echo     heltec_v4      Heltec WiFi LoRa 32 V4 + GPS   (8MB)
echo     t3_s3          LilyGO T3-S3 V1.2/V1.3         (4MB)
echo     tbeam_supreme  LilyGO T-Beam Supreme           (8MB)
echo.
