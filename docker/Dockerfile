# Toolchain image for building OpenGD77 (MD-UV380 / Retevis RT-3S) firmware.
#
# Pinned to linux/amd64 because the project's codec_cleaner tool ships only as
# an x86-64 Linux ELF (tools/codec_cleaner.Linux). On Apple Silicon this runs
# under Docker's built-in emulation (Rosetta/qemu). Keeping the whole build on
# one architecture avoids mixing native/emulated toolchains.
FROM --platform=linux/amd64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# IMPORTANT: use the same GCC major version as the official OpenGD77 builds.
# Ubuntu 22.04's stock gcc-arm-none-eabi is GCC 10.3.1, which miscompiles the
# timing-critical DMR / HR-C6000 path: the radio hard-faults (FreeRTOS
# configASSERT -> disable IRQs -> spin forever, needs a battery pull) the moment
# the AMBE codec runs on DMR TX. FM is unaffected. The official firmware is built
# with "gnu-tools-for-stm32 14.3.rel1" (GCC 14.3); we pin the equivalent vanilla
# Arm GNU Toolchain (GCC 14.2) so our build matches.
ARG ARM_TOOLCHAIN_VERSION=14.2.rel1
ARG ARM_TOOLCHAIN_TARBALL=arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz
ARG ARM_TOOLCHAIN_URL=https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TOOLCHAIN_VERSION}/binrel/${ARM_TOOLCHAIN_TARBALL}

RUN apt-get update && apt-get install -y --no-install-recommends \
        make \
        git \
        wget \
        xz-utils \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Download and install the Arm GNU Toolchain (bare-metal, arm-none-eabi).
RUN wget -q -O /tmp/arm-toolchain.tar.xz "${ARM_TOOLCHAIN_URL}" \
    && mkdir -p /opt/arm-toolchain \
    && tar -xJf /tmp/arm-toolchain.tar.xz -C /opt/arm-toolchain --strip-components=1 \
    && rm /tmp/arm-toolchain.tar.xz

ENV PATH="/opt/arm-toolchain/bin:${PATH}"

# Fail the image build early if the toolchain is not the expected GCC major.
RUN arm-none-eabi-gcc --version | grep -q ' 14\.' \
    && echo "Toolchain OK: $(arm-none-eabi-gcc --version | head -1)"

# git refuses to report a version for a repo owned by another user (the mounted
# /work). Trust it so the build stamps a real GitID instead of #UNKNOWN.
RUN git config --system --add safe.directory '*'

WORKDIR /work
