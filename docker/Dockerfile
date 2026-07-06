# Toolchain image for building OpenGD77 (MD-UV380 / Retevis RT-3S) firmware.
#
# Pinned to linux/amd64 because the project's codec_cleaner tool ships only as
# an x86-64 Linux ELF (tools/codec_cleaner.Linux). On Apple Silicon this runs
# under Docker's built-in emulation (Rosetta/qemu). Keeping the whole build on
# one architecture avoids mixing native/emulated toolchains.
FROM --platform=linux/amd64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi \
        binutils-arm-none-eabi \
        libnewlib-arm-none-eabi \
        libstdc++-arm-none-eabi-newlib \
        make \
        git \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
