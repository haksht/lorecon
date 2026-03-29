# ESP32 LoRa Sniffer - Flash Script (Windows PowerShell)
# Usage: .\flash.ps1 <board> [port]
#
# Boards: heltec_v3 | heltec_v4 | t3_s3 | tbeam_supreme
# Port:   auto-detected if omitted
#
# Examples:
#   .\flash.ps1 heltec_v3
#   .\flash.ps1 heltec_v3 COM3
#   .\flash.ps1 heltec_v4 COM12
#   .\flash.ps1 t3_s3 COM9
#   .\flash.ps1 tbeam_supreme COM11
#
# Requires: pip install esptool

param(
    [Parameter(Position=0)]
    [string]$Board = "",
    [Parameter(Position=1)]
    [string]$Port = ""
)

$boardConfigs = @{
    "heltec_v3"     = @{ Label = "Heltec WiFi LoRa 32 V3";       FlashSize = "8MB"; PortHint = "Usually COM3 (CP210x - check Device Manager)" }
    "heltec_v4"     = @{ Label = "Heltec WiFi LoRa 32 V4 (GPS)"; FlashSize = "8MB"; PortHint = "Usually COM12 (native USB - hold BOOT button if needed)" }
    "t3_s3"         = @{ Label = "LilyGO T3-S3 V1.2/V1.3";       FlashSize = "4MB"; PortHint = "Usually COM9 (native USB - hold BOOT button if needed)" }
    "tbeam_supreme" = @{ Label = "LilyGO T-Beam Supreme";         FlashSize = "8MB"; PortHint = "Usually COM11 (native USB - hold BOOT button if needed)" }
}

if ($Board -eq "" -or -not $boardConfigs.ContainsKey($Board)) {
    Write-Host "ERROR: Unknown board '$Board'" -ForegroundColor Red
    Write-Host ""
    Write-Host "Usage: .\flash.ps1 <board> [port]"
    Write-Host "Boards:"
    Write-Host "  heltec_v3      - Heltec WiFi LoRa 32 V3"
    Write-Host "  heltec_v4      - Heltec WiFi LoRa 32 V4 (GPS)"
    Write-Host "  t3_s3          - LilyGO T3-S3 V1.2/V1.3"
    Write-Host "  tbeam_supreme  - LilyGO T-Beam Supreme"
    exit 1
}

$cfg       = $boardConfigs[$Board]
$label     = $cfg.Label
$flashSize = $cfg.FlashSize
$portHint  = $cfg.PortHint

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$bin = Join-Path $scriptDir "$Board\full.bin"

if (-not (Test-Path $bin)) {
    Write-Host "ERROR: Binary not found: $bin" -ForegroundColor Red
    Write-Host "Make sure you extracted the full release zip."
    exit 1
}

# Auto-detect COM port if not specified
if ($Port -eq "") {
    Write-Host "Detecting COM port..."
    Add-Type -AssemblyName System.IO.Ports
    $ports = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($ports.Count -gt 0) {
        $Port = $ports[0]
        Write-Host "Found: $Port"
    } else {
        Write-Host "ERROR: No COM port detected." -ForegroundColor Red
        Write-Host "Hint: $portHint"
        Write-Host "Try: .\flash.ps1 $Board COM3"
        exit 1
    }
}

Write-Host ""
Write-Host "============================================"
Write-Host "  ESP32 LoRa Sniffer Flasher"
Write-Host "  Board: $label"
Write-Host "  Port:  $Port"
Write-Host "============================================"
Write-Host ""
Write-Host "Flashing full image (this takes 30-90 seconds)..."
Write-Host ""

python -m esptool --chip esp32s3 --port $Port --baud 921600 write_flash --flash_size $flashSize 0x0 $bin

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Flash FAILED. Try:" -ForegroundColor Red
    Write-Host "  1. Hold BOOT button while plugging in USB, then run again"
    Write-Host "  2. Specify port: .\flash.ps1 $Board COM3"
    Write-Host "  3. Lower baud: edit this script, change 921600 to 115200"
    Write-Host "  4. Install esptool: pip install esptool"
    exit 1
}

Write-Host ""
Write-Host "============================================"
Write-Host "  SUCCESS! Device flashed." -ForegroundColor Green
Write-Host "============================================"
Write-Host ""
Write-Host "Next steps:"
Write-Host "  1. Power-cycle the device (unplug and replug USB)"
Write-Host "  2. Connect to WiFi AP: LoRa-XXYYZZ"
Write-Host "  3. Password: recon-XXYYZZ  (matches SSID suffix)"
Write-Host "  4. Open browser: http://192.168.4.1"
Write-Host ""
