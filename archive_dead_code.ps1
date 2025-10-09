# Archive dead code files
# Run this from the project root: .\archive_dead_code.ps1

$archiveDir = "firmware\src\archive"

# Files to archive
$filesToArchive = @(
    "firmware\src\error_handler.cpp.broken",
    "firmware\src\hardware_stress_tester.cpp.old",
    "firmware\src\main_complex.cpp.backup",
    "firmware\src\main_old_phase2.cpp",
    "firmware\src\psk_decryption_clean.cpp",
    "firmware\src\psk_decryption_simple.cpp.backup",
    "firmware\src\psk_decryption_simple.cpp.manual_parsing_backup",
    "firmware\src\psk_decryption_simple.cpp.new"
)

Write-Host "Archiving dead code files to $archiveDir..." -ForegroundColor Cyan

foreach ($file in $filesToArchive) {
    if (Test-Path $file) {
        Write-Host "  Moving $file" -ForegroundColor Yellow
        Move-Item $file $archiveDir -Force
    } else {
        Write-Host "  Skipping $file (not found)" -ForegroundColor Gray
    }
}

Write-Host "`nDone! Files archived to $archiveDir" -ForegroundColor Green
