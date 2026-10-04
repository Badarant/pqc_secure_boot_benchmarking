# AM243x-LP test application - bare-metal Cortex-R5F, raw MSRAM binary.
# Built with the GNU arm-none-eabi toolchain (no SDK, no libc).
#
#   make                 -> out/app.elf, out/app.bin, out/app.map
#   make clean
#
# CGT_GCC_ARM_PATH may point at the toolchain root (default: on PATH).

ifdef CGT_GCC_ARM_PATH
  GCCBIN := $(CGT_GCC_ARM_PATH)/bin/
endif
CC      := $(GCCBIN)arm-none-eabi-gcc
OBJCOPY := $(GCCBIN)arm-none-eabi-objcopy
SIZE    := $(GCCBIN)arm-none-eabi-size

ARCH    := -mcpu=cortex-r5 -marm -mfloat-abi=soft
CFLAGS  := $(ARCH) -O2 -g3 -Wall -Wextra -ffreestanding -fno-common \
           -ffunction-sections -fdata-sections
LDFLAGS := $(ARCH) -T linker.ld -nostdlib -nostartfiles -static \
           -Wl,--gc-sections -Wl,-Map=out/app.map -Wl,--build-id=none

OBJDIR  := out

all: $(OBJDIR)/app.bin

$(OBJDIR):
	@mkdir -p $(OBJDIR)

$(OBJDIR)/app.elf: startup.S main.c linker.ld | $(OBJDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) startup.S main.c -o $@
	$(SIZE) $@

$(OBJDIR)/app.bin: $(OBJDIR)/app.elf
	$(OBJCOPY) -O binary $< $@
	@echo "app.bin: $$(wc -c < $@) bytes  -> load at BOOT_LOAD_ADDR ($$(grep -oE 'ORIGIN = 0x[0-9A-Fa-f]+' linker.ld | head -1 | cut -d' ' -f3))"

clean:
	rm -rf $(OBJDIR)

.PHONY: all clean
