# Makefile for the OpenGD77 MD-UV380 / Retevis RT-3S firmware (MDUV380_FW config).
#
# This reproduces the STM32CubeIDE "MDUV380_FW" build configuration (extracted
# from MDUV380_firmware/.cproject) as a plain Makefile so the firmware can be
# built headlessly.
#
# It is intended to be invoked with the current working directory set to a
# build directory that is a *direct child* of MDUV380_firmware/ (so that "../"
# resolves to the project root), exactly like CubeIDE's generated makefiles.
# The .incbin path inside codec_bin.S ("../application/source/linkerdata/...")
# relies on this layout.

PREFIX  := arm-none-eabi-
CC      := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE    := $(PREFIX)size

# ---- Platform selection ---------------------------------------------------
# PLATFORM=MDUV380 (default) -> TYT MD-UV380 / Retevis RT-3S
# PLATFORM=DM1701           -> Baofeng DM-1701 / Retevis RT-84
PLATFORM ?= MDUV380
ifeq ($(PLATFORM),DM1701)
PLATFORM_DEFS := -DPLATFORM_RT84_DM1701 -DPLATFORM_VARIANT_DM1701
TARGET        := OpenDM1701
else
PLATFORM_DEFS := -DPLATFORM_MDUV380
TARGET        := OpenMDUV380
endif

# ---- MCU / FPU ------------------------------------------------------------
CPU := -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb

# ---- Preprocessor defines -------------------------------------------------
# NDEBUG + DEBUG mirrors the STM32CubeIDE release configs exactly.
DEFS := -DUSE_HAL_DRIVER -DSTM32F405xx $(PLATFORM_DEFS) -DNDEBUG -DDEBUG

# Short git hash if available, otherwise UNKNOWN (matches the .cproject rule).
GITVERSION := $(shell git -C .. rev-parse --short HEAD 2>/dev/null || echo UNKNOWN)

# ---- Include paths (relative to build dir; ../ == project root) ------------
INCLUDES := \
	-I../Middlewares/Third_Party/FreeRTOS/Source/include \
	-I../application/include \
	-I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
	-I../USB_DEVICE/Target \
	-I../Drivers/CMSIS/Include \
	-I../Core/Inc \
	-I../Drivers/STM32F4xx_HAL_Driver/Inc \
	-I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc \
	-I../USB_DEVICE/App \
	-I../Drivers/CMSIS/Device/ST/STM32F4xx/Include \
	-I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
	-I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc \
	-I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy \
	-I../

# ---- Flags ----------------------------------------------------------------
WARN    := -Wall -Wno-format -Wno-format-truncation -Wno-stringop-overflow
CFLAGS  := $(CPU) -std=gnu11 $(DEFS) -DGITVERSION=$(GITVERSION) $(INCLUDES) \
	-Os -ffunction-sections -fdata-sections $(WARN) -fstack-usage -g3 \
	-MMD -MP
ASFLAGS := $(CPU) -x assembler-with-cpp -DDEBUG -g3 -MMD -MP
LDSCRIPT := ../STM32F405VGTX_FLASH.ld
LDFLAGS := $(CPU) -T$(LDSCRIPT) --specs=nano.specs \
	-Wl,-Map=$(TARGET).map,--cref -Wl,--gc-sections -static \
	-Wl,--print-memory-usage \
	-Wl,--start-group -lc -lm -Wl,--end-group

# ---- Sources --------------------------------------------------------------
# All .c under the CubeIDE source paths, excluding the host-side language
# builder (which is excluded in the MDUV380_FW config and defines its own main).
C_SRCS := $(shell find ../Core ../Drivers ../Middlewares ../USB_DEVICE ../SeggerRTT ../application \
	-name '*.c' ! -name 'languages_builder.c')
ASM_SRCS := $(shell find ../Core ../SeggerRTT ../application \
	\( -name '*.s' -o -name '*.S' \))

OBJS := $(patsubst ../%,obj/%.o,$(C_SRCS) $(ASM_SRCS))
DEPS := $(OBJS:.o=.d)

# ---- Rules ----------------------------------------------------------------
.PHONY: all size
all: $(TARGET).bin size

obj/%.c.o: ../%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.s.o: ../%.s
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

obj/%.S.o: ../%.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Raw binary straight from the ELF...
$(TARGET)_raw.bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# ...then run codec_cleaner (post-build step from .cproject) to clear the
# codec regions, producing the final flashable image.
$(TARGET).bin: $(TARGET)_raw.bin
	../tools/codec_cleaner.Linux -i $< -o $@

size: $(TARGET).elf
	$(SIZE) $(TARGET).elf

-include $(DEPS)
