#!/usr/bin/env bash
#
# Build the OpenGD77 firmware for the Retevis RT-3S / TYT MD-UV380 in Docker.
# Nothing is installed on the host: the ARM toolchain lives only in the image,
# and the build output lands in MDUV380_firmware/build/OpenMDUV380.bin.
#
# Usage:  ./build-firmware.sh
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE=opengd77-mduv380-builder

echo "==> Building toolchain image ($IMAGE)"
docker build --platform linux/amd64 -t "$IMAGE" "$REPO_DIR/docker"

echo "==> Running build in container"
docker run --rm --platform linux/amd64 \
	-v "$REPO_DIR":/work \
	"$IMAGE" \
	bash /work/docker/build.sh

echo
echo "==> Firmware ready:"
echo "    $REPO_DIR/MDUV380_firmware/build/OpenMDUV380.bin"
echo
echo "Flash it (with the radio in firmware-update mode) using:"
echo "    ./MDUV380_firmware/tools/opengd77_stm32_firmware_loader.py \\"
echo "        -m MD-UV380 -f MDUV380_firmware/build/OpenMDUV380.bin"
