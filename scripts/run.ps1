param(
  [string]$BuildDir = "build",
  [ValidateSet("gui", "stdio")]
  [string]$Mode = "gui"
)

$iso = Join-Path $BuildDir "mgcore.iso"
if (-not (Test-Path $iso)) {
  Write-Error "ISO not found at $iso. Build with: cmake -S . -B $BuildDir && cmake --build $BuildDir"
  Write-Host "On Windows/MSYS2, prepare Limine first with: ./scripts/setup-limine.ps1"
  exit 1
}

if ($Mode -eq "gui") {
  $serialLog = Join-Path $BuildDir "serial.log"
  New-Item -ItemType File -Force -Path $serialLog | Out-Null
  $qemu = (Get-Command "qemu-system-x86_64w.exe" -ErrorAction SilentlyContinue)
  if (-not $qemu) {
    $qemu = (Get-Command "qemu-system-x86_64.exe" -ErrorAction Stop)
  }
  & $qemu.Source `
    -m 256M `
    -display sdl `
    -netdev user,id=n0 `
    -device rtl8139,netdev=n0 `
    -serial "file:$serialLog" `
    -cdrom $iso `
    -no-reboot `
    -d guest_errors
  exit $LASTEXITCODE
}

& qemu-system-x86_64.exe `
  -m 256M `
  -netdev user,id=n0 `
  -device rtl8139,netdev=n0 `
  -serial stdio `
  -cdrom $iso `
  -no-reboot `
  -d guest_errors
