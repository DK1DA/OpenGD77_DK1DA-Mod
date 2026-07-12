#!/usr/bin/env bash
#
# Grab a screenshot from the radio over USB (CDC serial), using the OpenGD77
# screen grabber. Runs natively (USB serial is not available inside Docker) and
# sets up a self-contained Python venv (pyserial + pillow) on first use.
#
# The radio must be powered on, running the firmware, and connected via USB.
#
# Usage:
#   ./screenshot.sh                       # auto-detect port -> screenshots/screenshot-<timestamp>.png
#   ./screenshot.sh vfo-7seg              # -> screenshots/vfo-7seg.png
#   ./screenshot.sh -d /dev/cu.usbmodem1234 [name]   # explicit serial port
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GRABBER="$REPO_DIR/MDUV380_firmware/tools/gd-77_screen_grabber.py"
VENV="$REPO_DIR/MDUV380_firmware/tools/.venv"
OUT_DIR="$REPO_DIR/screenshots"

# ---- optional explicit device (-d /dev/...) -------------------------------
DEVICE=""
if [[ "${1:-}" == "-d" ]]; then
	DEVICE="${2:-}"; shift 2 || true
fi

# ---- auto-detect the radio's serial port ----------------------------------
if [[ -z "$DEVICE" ]]; then
	case "$(uname -s)" in
		Darwin) DEVICE="$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)" ;;
		*)      DEVICE="$(ls /dev/ttyACM* 2>/dev/null | head -1 || true)" ;;
	esac
fi
if [[ -z "$DEVICE" ]]; then
	echo "!! No radio serial port found." >&2
	echo "   Connect the radio via USB (powered on, running firmware) and retry, or pass one:" >&2
	echo "     ./screenshot.sh -d /dev/cu.usbmodemXXXX" >&2
	exit 1
fi

# ---- one-time Python venv with the grabber's dependencies ------------------
if [[ ! -x "$VENV/bin/python" ]]; then
	echo "==> First run: creating Python venv with pyserial + pillow..."
	python3 -m venv "$VENV"
	"$VENV/bin/pip" install --quiet --upgrade pip
	"$VENV/bin/pip" install --quiet pyserial pillow
fi

# ---- output name -----------------------------------------------------------
mkdir -p "$OUT_DIR"
NAME="${1:-screenshot-$(date +%Y%m%d-%H%M%S)}"
OUT="$OUT_DIR/$NAME"

echo "==> Grabbing from $DEVICE"
"$VENV/bin/python" "$GRABBER" -d "$DEVICE" -o "$OUT"
echo "==> Saved: $OUT.png"
