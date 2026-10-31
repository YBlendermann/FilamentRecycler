#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
UPSTREAM="${ROOT}/.vendor/elecrow-5"
SRC="${UPSTREAM}/example/V3.0/Arduino/Course/LVGL_Arduino5.0"
DST="${ROOT}/.build/LVGL_Arduino5.0"
if [ ! -f "${UPSTREAM}/readme.md" ]; then
  mkdir -p "$(dirname "$UPSTREAM")"
  git clone --depth 1 https://github.com/Elecrow-RD/CrowPanel-5.0-HMI-ESP32-Display-800x480.git "$UPSTREAM"
fi
if [ ! -f "$SRC/LVGL_Arduino5.0.ino" ]; then
  echo "Elecrow V3.0-Beispiel nicht gefunden: $SRC" >&2
  exit 1
fi
mkdir -p "${ROOT}/.build"
rm -rf "$DST"
cp -a "$SRC" "$DST"
cp "${ROOT}/firmware/crowpanel/filament_temp_ui.h" "$DST/filament_temp_ui.h"
python3 "${ROOT}/scripts/patch_elecrow.py" "$DST/LVGL_Arduino5.0.ino"
echo "CrowPanel-Quellcode vorbereitet: $DST"
