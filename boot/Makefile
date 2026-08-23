CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
SIZE=arm-none-eabi-size

PQM4    = ../third_party_sw/pqm4
MUPQCOMMON  = $(PQM4)/mupq/common
PQCLEAN = $(PQM4)/mupq/pqclean
PCOMMON = $(PQCLEAN)/common
MCOMMON = $(PQM4)/common
PQM4COMMON  = $(PQM4)/common

CFLAGS_BASE = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
              -O2 -g3 -Wall -Wextra -ffreestanding -nostartfiles
LDFLAGS = -T linker.ld -nostartfiles --specs=nosys.specs -Wl,--gc-sections -ffunction-sections -fdata-sections

FREQ ?= 4                          # default: MSI 4 MHz (no PLL)
FREQ_DEF = $(if $(filter 4,$(FREQ)),,-DCPU_FREQ_$(FREQ))

# default: SHA3_256
HASH ?= SHA3_256
HASH_DEF = -DHASH_$(HASH)

LOCAL_SRC = startup.c main.c uart.c clock.c

# ---- surse per implementare (depind de $(SCHEME)) ----
CLEAN_DIR = $(PQCLEAN)/crypto_sign/$(SCHEME)/clean
CLEAN_SRC = $(CLEAN_DIR)/sign.c $(CLEAN_DIR)/packing.c $(CLEAN_DIR)/poly.c \
            $(CLEAN_DIR)/polyvec.c $(CLEAN_DIR)/ntt.c $(CLEAN_DIR)/reduce.c \
            $(CLEAN_DIR)/rounding.c $(CLEAN_DIR)/symmetric-shake.c \
            $(PCOMMON)/fips202.c $(MUPQCOMMON)/keccakf1600.c

M4F_DIR = $(PQM4)/crypto_sign/$(SCHEME)/m4f
M4F_SRC = $(M4F_DIR)/sign.c $(M4F_DIR)/packing.c $(M4F_DIR)/poly.c \
          $(M4F_DIR)/polyvec.c $(M4F_DIR)/rounding.c $(M4F_DIR)/smallpoly.c \
          $(M4F_DIR)/symmetric-shake.c \
          $(M4F_DIR)/ntt.S $(M4F_DIR)/pointwise_mont.s \
          $(M4F_DIR)/smallntt_769.S $(M4F_DIR)/vector.s \
          $(MUPQCOMMON)/fips202.c $(PQM4COMMON)/keccakf1600.S $(MUPQCOMMON)/sha2.c 

# ---- selectie in functie de IMPL (variabile recursive, expandate la recipe) ----
SRC     = $(if $(filter clean,$(IMPL)),$(CLEAN_SRC),$(M4F_SRC))
IMPL_INC = $(if $(filter clean,$(IMPL)), \
             -I$(CLEAN_DIR) -I$(PCOMMON), \
             -I$(M4F_DIR) -I$(MUPQCOMMON) -I$(PQM4COMMON))
IMPL_DEF= $(if $(filter clean,$(IMPL)),-DIMPL_CLEAN,-DIMPL_M4F)
SCHEME_DEF = $(if $(filter ml-dsa-44,$(SCHEME)),-DSCHEME_44,$(if $(filter ml-dsa-65,$(SCHEME)),-DSCHEME_65,-DSCHEME_87))

OUT = boot-$(IMPL)-$(SCHEME)-$(HASH)-$(FREQ)MHz

# ==== benchmarking ====

bench-clean-44: IMPL=clean
bench-clean-44: SCHEME=ml-dsa-44
bench-m4f-44:   IMPL=m4f
bench-m4f-44:   SCHEME=ml-dsa-44
bench-clean-65: IMPL=clean
bench-clean-65: SCHEME=ml-dsa-65
bench-m4f-65:   IMPL=m4f
bench-m4f-65:   SCHEME=ml-dsa-65
bench-clean-87: IMPL=clean
bench-clean-87: SCHEME=ml-dsa-87
bench-m4f-87:   IMPL=m4f
bench-m4f-87:   SCHEME=ml-dsa-87

bench-clean-44 bench-m4f-44 bench-clean-65 bench-m4f-65 bench-clean-87 bench-m4f-87: _build

_build:
	$(CC) $(CFLAGS_BASE) $(IMPL_DEF) $(SCHEME_DEF) $(HASH_DEF) $(FREQ_DEF) $(IMPL_INC) $(LDFLAGS) \
	  $(LOCAL_SRC) $(SRC) -o $(OUT).elf
	$(OBJCOPY) -O binary $(OUT).elf $(OUT).bin 
	$(SIZE) $(OUT).elf 
	@echo ">>> built $(OUT).bin"

clean-build:
	rm -f boot-*.elf boot-*.bin boot-*.map