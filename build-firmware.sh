#!/usr/bin/env bash
#
# Build the OpenGD77_DK1DA-Mod firmware in Docker. Nothing is installed on the host: the
# ARM toolchain lives only in the image.
#
# Usage:  ./build-firmware.sh [MDUV380|DM1701]   (default: MDUV380)
#   MDUV380 -> TYT MD-UV380 / Retevis RT-3S     -> MDUV380_firmware/build/OpenMDUV380.bin
#   DM1701  -> Baofeng DM-1701 / Retevis RT-84  -> MDUV380_firmware/build/OpenDM1701.bin
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE=opengd77-mduv380-builder

PLATFORM="${1:-MDUV380}"
case "$PLATFORM" in
	MDUV380) OUT="OpenMDUV380"; MODEL="MD-UV380" ;;
	DM1701)  OUT="OpenDM1701";  MODEL="DM-1701"  ;;
	*) echo "Unknown platform '$PLATFORM' (use MDUV380 or DM1701)"; exit 1 ;;
esac

echo "==> Building toolchain image ($IMAGE)"
docker build --platform linux/amd64 -t "$IMAGE" "$REPO_DIR/docker"

echo "==> Running build in container ($PLATFORM)"
BUILD_LOG="$(mktemp)"
trap 'rm -f "$BUILD_LOG"' EXIT
# Stream to the terminal and capture a copy so we can re-show the linker's
# memory-usage table at the end (pipefail makes a build failure still abort).
docker run --rm --platform linux/amd64 \
	-v "$REPO_DIR":/work \
	"$IMAGE" \
	bash /work/docker/build.sh "$PLATFORM" 2>&1 | tee "$BUILD_LOG"

echo
echo "==> Firmware ready:"
echo "    $REPO_DIR/MDUV380_firmware/build/${OUT}.bin"

# Re-print the linker's per-region memory usage (emitted by --print-memory-usage
# during the link) with the free bytes worked out for each region.
if grep -q "Memory region" "$BUILD_LOG"; then
	echo
	echo "==> Memory usage (FLASH = code budget, RAM/CCMRAM = data budget):"
	grep -A3 "Memory region" "$BUILD_LOG" | awk '
		function toBytes(n, unit) {
			if (unit == "KB") return n * 1024
			if (unit == "MB") return n * 1024 * 1024
			if (unit == "GB") return n * 1024 * 1024 * 1024
			return n # already bytes
		}
		NR == 1 { printf "    %-9s %11s %12s %8s %11s\n", "Region", "Used", "Size", "Used%", "Free"; next }
		/:/ {
			gsub(":", "", $1)
			used = toBytes($2, $3); size = toBytes($4, $5); free = size - used
			printf "    %-9s %9d B %10d B %7s %9d B\n", $1, used, size, $6, free
		}'
fi
echo
echo "Flash it (with the radio in firmware-update mode) using:"
echo "    ./MDUV380_firmware/tools/opengd77_stm32_firmware_loader.py \\"
echo "        -m ${MODEL} -f MDUV380_firmware/build/${OUT}.bin"
