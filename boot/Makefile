CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

COMMON = ../third_party_sw/pqm4/mupq/pqclean/common

MLDSA = ../third_party_sw/pqm4/mupq/pqclean/crypto_sign/ml-dsa-65/clean

MLDSA_SRC = $(MLDSA)/sign.c $(MLDSA)/packing.c $(MLDSA)/poly.c \
            $(MLDSA)/polyvec.c $(MLDSA)/ntt.c $(MLDSA)/reduce.c \
            $(MLDSA)/rounding.c $(MLDSA)/symmetric-shake.c

MLDSA_SRC += $(COMMON)/fips202.c

LOCAL_SRC = startup.c main.c alloc.c uart.c

CFLAGS  = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
          -O2 -g3 -Wall -Wextra -ffreestanding -nostartfiles -I$(MLDSA)

CFLAGS += -I$(COMMON)

LDFLAGS = -T linker.ld -nostartfiles -Wl,--gc-sections -Wl,-Map=boot.map --specs=nosys.specs

all: boot.bin

boot.elf: $(LOCAL_SRC) $(MLDSA_SRC) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(LOCAL_SRC) $(MLDSA_SRC) -o $@
	$(SIZE) $@

boot.bin: boot.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f boot.elf boot.bin boot.map