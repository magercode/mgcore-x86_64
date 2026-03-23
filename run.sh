#!/usr/bin/env sh
set -eu

BUILD_DIR="${1:-build}"
MODE="${2:-gui}"
ISO="${BUILD_DIR}/mgcore.iso"
SERIAL_LOG="${BUILD_DIR}/serial.log"

if [ ! -f "${ISO}" ]; then
  echo "ISO not found at ${ISO}. Build with: cmake -S . -B ${BUILD_DIR} && cmake --build ${BUILD_DIR}"
  echo "On Windows/MSYS2, prepare Limine first with: ./scripts/setup-limine.sh"
  exit 1
fi

if [ "${MODE}" = "gui" ]; then
  QEMU_BIN="${QEMU_BIN:-qemu-system-x86_64w.exe}"
  if ! command -v "${QEMU_BIN}" >/dev/null 2>&1; then
    QEMU_BIN="${QEMU_BIN_FALLBACK:-qemu-system-x86_64.exe}"
  fi
  : > "${SERIAL_LOG}"
  exec "${QEMU_BIN}" \
    -m 256M \
    -display sdl \
    -netdev user,id=n0 \
    -device rtl8139,netdev=n0 \
    -serial "file:${SERIAL_LOG}" \
    -cdrom "${ISO}" \
    -no-reboot \
    -d guest_errors
fi

exec qemu-system-x86_64.exe \
  -m 256M \
  -netdev user,id=n0 \
  -device rtl8139,netdev=n0 \
  -serial stdio \
  -cdrom "${ISO}" \
  -no-reboot \
  -d guest_errors
