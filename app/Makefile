CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

CFLAGS  = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
          -O2 -g3 -Wall -Wextra -ffreestanding -nostdlib
LDFLAGS = -T linker.ld -nostdlib -Wl,--gc-sections -Wl,-Map=app.map

all: app.bin

app.elf: startup.c main.c linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) startup.c main.c -o $@
	$(SIZE) $@

app.bin: app.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f app.elf app.bin app.map
