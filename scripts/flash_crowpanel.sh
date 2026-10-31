#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="${ARDUINO_CLI:-$HOME/arduino-tools/bin/arduino-cli}"
PORT="${1:-/dev/ttyUSB0}"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=4M"
bash "${ROOT}/scripts/prepare_crowpanel.sh"
# Herstellerbibliotheken bewusst bevorzugen, um Versionskonflikte zu vermeiden.
LIBS="${ROOT}/.vendor/elecrow-5/example/V3.0/Arduino/libraries"
if [ ! -d "$LIBS" ]; then
  echo "Herstellerbibliotheken fehlen: $LIBS" >&2
  exit 1
fi
"$CLI" core list
"$CLI" compile --fqbn "$FQBN" --libraries "$LIBS" \
  "$ROOT/.build/LVGL_Arduino5.0"
"$CLI" upload --fqbn "$FQBN" -p "$PORT" \
  "$ROOT/.build/LVGL_Arduino5.0"
