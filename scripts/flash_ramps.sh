#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="${ARDUINO_CLI:-$HOME/arduino-tools/bin/arduino-cli}"
PORT="${1:-/dev/ttyACM0}"
"$CLI" compile --fqbn arduino:avr:mega "$ROOT/firmware/ramps/mega_temp_usb"
"$CLI" upload -p "$PORT" --fqbn arduino:avr:mega "$ROOT/firmware/ramps/mega_temp_usb"
