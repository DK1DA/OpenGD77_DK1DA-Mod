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
docker run --rm --platform linux/amd64 \
	-v "$REPO_DIR":/work \
	"$IMAGE" \
	bash /work/docker/build.sh "$PLATFORM"

echo
echo "==> Firmware ready:"
echo "    $REPO_DIR/MDUV380_firmware/build/${OUT}.bin"
echo
echo "Flash it (with the radio in firmware-update mode) using:"
echo "    ./MDUV380_firmware/tools/opengd77_stm32_firmware_loader.py \\"
echo "        -m ${MODEL} -f MDUV380_firmware/build/${OUT}.bin"
