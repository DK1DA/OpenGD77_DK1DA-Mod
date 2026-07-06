#!/usr/bin/env bash
#
# In-container build script. Runs inside the toolchain image with the repo
# mounted at /work. Produces MDUV380_firmware/build/Open<PLATFORM>.bin.
#
# Usage: build.sh [MDUV380|DM1701]   (default: MDUV380)
#   MDUV380 -> TYT MD-UV380 / Retevis RT-3S  -> OpenMDUV380.bin
#   DM1701  -> Baofeng DM-1701 / Retevis RT-84 -> OpenDM1701.bin
set -euo pipefail

PLATFORM="${1:-MDUV380}"
case "$PLATFORM" in
	MDUV380) OUT="OpenMDUV380" ;;
	DM1701)  OUT="OpenDM1701"  ;;
	*) echo "Unknown platform '$PLATFORM' (use MDUV380 or DM1701)"; exit 1 ;;
esac

cd /work/MDUV380_firmware

echo "==> Building for $PLATFORM"
echo "==> Preparing codec section (codec_cleaner -C)"
chmod +x tools/codec_cleaner.Linux
( cd application/source/linkerdata && ../../../tools/codec_cleaner.Linux -C )

echo "==> Building firmware"
# Clean build: obj/ lives on the host mount and would otherwise be reused across
# toolchain/platform changes, silently relinking stale .o files.
rm -rf build
mkdir -p build
cd build
make -f /work/docker/firmware.mk PLATFORM="$PLATFORM" -j"$(nproc)"

echo
echo "==> Build complete:"
ls -l "/work/MDUV380_firmware/build/${OUT}.bin"
