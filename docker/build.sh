#!/usr/bin/env bash
#
# In-container build script. Runs inside the toolchain image with the repo
# mounted at /work. Produces MDUV380_firmware/build/OpenMDUV380.bin
# (the flashable OpenGD77 image for the MD-UV380 / Retevis RT-3S).
set -euo pipefail

cd /work/MDUV380_firmware

echo "==> Preparing codec section (codec_cleaner -C)"
chmod +x tools/codec_cleaner.Linux
( cd application/source/linkerdata && ../../../tools/codec_cleaner.Linux -C )

echo "==> Building firmware"
# Clean build: obj/ lives on the host mount and would otherwise be reused across
# toolchain changes, silently relinking stale .o files (e.g. an old GCC build).
rm -rf build
mkdir -p build
cd build
make -f /work/docker/firmware.mk -j"$(nproc)"

echo
echo "==> Build complete:"
ls -l /work/MDUV380_firmware/build/OpenMDUV380.bin
