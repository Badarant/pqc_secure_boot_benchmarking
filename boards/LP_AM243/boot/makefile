#
# boot - AM243x-LP / r5fss0-0 / nortos / ti-arm-clang
#
#
# Built on the SDK's own sbl_ospi example up through loading/starting the
# configured application cores, but does NOT call the stock example's
# unconditional Bootloader_runSelfCpu() self-reset at the end (that resets
# R5F0-0 into whatever is preloaded in its own ATCM - with no R5F0-0 image in
# our multicore bundle, just a reset vector + a bare WFI - so R5F0-0 would
# silently sleep and never run the orchestration logic below).
#
# Standalone build (does not use the SDK per-example makefile machinery in
# imports.mak/devconfig.mak - self-sufficient, same style as boot/makefile).
#
# Usage (see scripts/build.bat for the Windows wrapper that sets tool paths):
#   gmake                       # release
#   gmake PROFILE=debug
#   gmake clean
#
# Required (env or make var): MCU_PLUS_SDK_PATH, CGT_TI_ARM_CLANG_PATH,
# SYSCFG_CLI (node + sysconfig cli.js), PYTHON (a real interpreter - see
# boot/makefile's own PYTHON note).
#

PROFILE ?= release
NAME    := boot
PYTHON  ?= python

CC  := $(CGT_TI_ARM_CLANG_PATH)/bin/tiarmclang
LNK := $(CGT_TI_ARM_CLANG_PATH)/bin/tiarmclang
OBJCOPY := $(CGT_TI_ARM_CLANG_PATH)/bin/tiarmobjcopy

# BOOT_CONFIG selects where the secure-boot crypto runs:
#   A (default) = both steps on the M4F enclave, requested over IPC: software
#                 SHA-256 (pqm4 mupq/common/sha2.c) then ML-DSA verify (pqm4
#                 m4f, hand-written Cortex-M4 assembly).
#   B           = both steps here on the R5F, in software, with no IPC anywhere
#                 in the boot path: the same pqm4 sha2.c the enclave runs in A,
#                 then ML-DSA verify from PQClean's portable-C "clean"
#                 implementation. The enclave is not used at all.
#
# Config A needs ../enclave/ built and flashed; config B does not use it at all.

# GNU Make's default goal is "the first target in the file" unless pinned. The
# FORCE/CONFIG_STAMP plumbing below defines FORCE: long before all:, so without
# this line a bare `gmake` / `build.bat` with no target builds FORCE - printing
# "Nothing to be done for 'FORCE'" and exiting 0 while producing NOTHING. The
# enclave makefile was fixed for this in an earlier session; this one was not,
# so every target-less boot build silently did nothing. Only `build.bat
# bootimage` (an explicit target, which bypasses the default-goal lookup) ever
# really built, which is why it went unnoticed.
.DEFAULT_GOAL := all

BOOT_CONFIG ?= A
ifeq ($(filter $(BOOT_CONFIG),A B),)
  $(error BOOT_CONFIG must be A or B)
endif

# Config B links one ML-DSA parameter set (see below for why not all three).
PQC_SCHEME ?= 87
ifeq ($(filter $(PQC_SCHEME),44 65 87),)
  $(error PQC_SCHEME must be 44, 65 or 87)
endif

# OBJDIR is keyed on the config, so each variant has its own object cache and a
# switch can never reuse objects compiled with the other setting. For B the
# scheme is part of that key - the PQClean object file names are identical
# across parameter sets, so without it a 65 build would silently reuse 87's.
CONFIG := $(PROFILE)-cfg$(BOOT_CONFIG)
ifeq ($(BOOT_CONFIG),B)
  CONFIG := $(CONFIG)-pqc$(PQC_SCHEME)
endif

# STACK_PAINT=1 adds the stack high-water measurement (../common/stack_paint.h):
# fill the free stack at main() entry, scan it before the verdict, print a
# [mem] line. Off by default so the timing builds carry nothing extra - the
# paint itself is outside every timed window, but it does dirty the D-cache
# before the first hash. For config A the enclave must be built with the SAME
# setting: it is what reports the M4F's own number over IPC. Config B has no
# enclave, so only this side matters there.
STACK_PAINT ?= 0
ifeq ($(STACK_PAINT),1)
  CONFIG := $(CONFIG)-paint
endif
OBJDIR := obj/$(CONFIG)
OUT    := $(NAME).$(PROFILE).out
MAP    := $(NAME).$(PROFILE).map

# $(OUT) is named by PROFILE only, so a config switch alone would not make it
# look out of date: the other config's objects are already built and older than
# the existing $(OUT), so make would skip the link and leave a binary built for
# the wrong config on disk - flashed, that is a silent wrong-benchmark run.
# This stamp's recipe always runs, but only rewrites the file (and so only bumps
# its mtime) when CONFIG actually changed, forcing exactly one relink then.
CONFIG_STAMP := obj/.config-$(PROFILE).stamp
.PHONY: FORCE
FORCE:
$(CONFIG_STAMP): FORCE | dirs
	@if [ ! -f $@ ] || [ "$$(cat $@ 2>/dev/null)" != "$(CONFIG)" ]; then \
		echo "$(CONFIG)" > $@; \
	fi

REPO_ROOT := $(abspath ../../..)
COMMON    := $(REPO_ROOT)/common
BOARD_COMMON := $(abspath ../common)

ARCH_FLAGS := -mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -mthumb

INCLUDES := \
	-I. \
	-Igenerated \
	-I$(COMMON) \
	-I$(BOARD_COMMON) \
	-I$(CGT_TI_ARM_CLANG_PATH)/include/c \
	-I$(MCU_PLUS_SDK_PATH)/source \

DEFINES := -DSOC_AM243X -DOS_NORTOS -DBOOT_CONFIG_$(BOOT_CONFIG)=1 \
           -DPQSB_STACK_PAINT=$(STACK_PAINT)

CFLAGS_debug   := -D_DEBUG_=1
CFLAGS_release := -Os
CFLAGS := $(ARCH_FLAGS) -g -Wall -Wextra -Wno-unused-parameter -std=c11 \
          $(CFLAGS_$(PROFILE))

FILES := \
	main.c \
	generated/ti_drivers_config.c \
	generated/ti_drivers_open_close.c \
	generated/ti_board_config.c \
	generated/ti_board_open_close.c \
	generated/ti_dpl_config.c \
	generated/ti_pinmux_config.c \
	generated/ti_power_clock_config.c

# Config B hashes in software on this core, using the SAME implementation the
# enclave uses (pqm4 mupq/common/sha2.c) so that the hash half
# of A vs B differs only by which core runs it, not by which SHA-256 it runs.
# The verify half cannot be held equal the same way - see the PQClean block
# below for why - so this is the one step where the comparison is apples to
# apples at source level.
PQM4_COMMON :=
ifeq ($(BOOT_CONFIG),B)
  PQM4_COMMON := $(REPO_ROOT)/third_party_sw/pqm4/mupq/common
  INCLUDES    += -I$(PQM4_COMMON)
  FILES       += $(PQM4_COMMON)/sha2.c
endif

OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(FILES)))
vpath %.c . generated $(PQM4_COMMON)

# ---- config B: PQClean portable-C ML-DSA, verified on this R5F -------------
PQC_DIRS :=
ifeq ($(BOOT_CONFIG),B)
  PQC_ROOT   := $(REPO_ROOT)/third_party_sw/pqm4/mupq/pqclean
  PQC_COMMON := $(PQC_ROOT)/common
  PQC_SRCDIR := $(PQC_ROOT)/crypto_sign/ml-dsa-$(PQC_SCHEME)/clean
  PQC_DIRS   := $(OBJDIR)/pqc

  PQC_SRCS := $(wildcard $(PQC_SRCDIR)/*.c)

  PQC_OBJS := $(patsubst %.c,$(OBJDIR)/pqc/%.o,$(notdir $(PQC_SRCS))) \
              $(OBJDIR)/pqc/fips202.o \
              $(OBJDIR)/pqc/pqclean_randombytes_stub.o

  OBJS     += $(PQC_OBJS)
  DEFINES  += -DPQC_SCHEME=$(PQC_SCHEME)
  # main.c includes the selected set's api.h by relative path, so no extra -I is
  # needed for it - and none is wanted: PQClean's params.h/config macros would
  # otherwise land on our own include path and collide. Nor can PQC_COMMON go on
  # the main INCLUDES list: PQClean's common/ carries its own fips202.h and
  # randombytes.h, and config B also compiles pqm4's sha2.c, whose headers must
  # not be shadowed.

  # pqclean_glue/ must come FIRST so its randombytes.h shadows PQClean's own,
  # which includes <unistd.h> and cannot be compiled bare-metal. Only signing
  # needs randombytes and boot never signs.
  PQC_INC := -I$(CURDIR)/pqclean_glue -I$(PQC_COMMON)
endif

# third-party sources build with our arch/optimisation settings but without our
# -W flags
CFLAGS_TP := $(ARCH_FLAGS) -g $(CFLAGS_$(PROFILE)) -std=c11

# ...except the crypto itself - pqm4's sha2.c and PQClean's ML-DSA, both config
# B - which is what the benchmark measures.
#
CFLAGS_CRYPTO := $(ARCH_FLAGS) -g -O2 -std=c11

LIB_PATHS := \
	-Wl,-i$(MCU_PLUS_SDK_PATH)/source/kernel/nortos/lib \
	-Wl,-i$(MCU_PLUS_SDK_PATH)/source/drivers/lib \
	-Wl,-i$(MCU_PLUS_SDK_PATH)/source/board/lib \
	-Wl,-i$(MCU_PLUS_SDK_PATH)/source/security/lib \
	-Wl,-i$(CGT_TI_ARM_CLANG_PATH)/lib

LIBS := \
	-lnortos.am243x.r5f.ti-arm-clang.$(PROFILE).lib \
	-ldrivers.am243x.r5f.ti-arm-clang.nortos.$(PROFILE).lib \
	-lboard.am243x.r5f.ti-arm-clang.nortos.$(PROFILE).lib \
	-lsecurity.am243x.r5f.ti-arm-clang.nortos.$(PROFILE).lib \
	-llibc.a \
	-llibsysbm.a

LFLAGS := $(ARCH_FLAGS) -Wl,--diag_suppress=10063 -Wl,--ram_model \
          -Wl,--reread_libs -Wl,--xml_link_info=$(NAME).$(PROFILE).lnkxml

# ==========================================================================
.PHONY: all clean scrub syscfg bootimage

all: $(OUT)

TRACKED_H := $(wildcard $(COMMON)/*.h) $(wildcard $(BOARD_COMMON)/*.h) $(wildcard *.h)

$(OBJS): $(TRACKED_H) makefile

$(OBJS): | generated/ti_drivers_config.h

$(OBJDIR)/%.o: %.c | dirs
	$(CC) -c $(CFLAGS) $(INCLUDES) $(DEFINES) -MMD -o $@ $<

$(OBJDIR)/sha2.o: $(PQM4_COMMON)/sha2.c | dirs
	$(CC) -c $(CFLAGS_CRYPTO) -Wno-unused-function $(INCLUDES) $(DEFINES) -MMD -o $@ $<

$(OBJDIR)/pqc/%.o: $(PQC_SRCDIR)/%.c | dirs
	$(CC) -c $(CFLAGS_CRYPTO) $(PQC_INC) $(DEFINES) -MMD -o $@ $<

$(OBJDIR)/pqc/fips202.o: $(PQC_COMMON)/fips202.c | dirs
	$(CC) -c $(CFLAGS_CRYPTO) $(PQC_INC) $(DEFINES) -MMD -o $@ $<

$(OBJDIR)/pqc/pqclean_randombytes_stub.o: pqclean_glue/pqclean_randombytes_stub.c | dirs
	$(CC) -c $(CFLAGS_TP) $(PQC_INC) $(INCLUDES) $(DEFINES) -MMD -o $@ $<

$(OUT): $(OBJS) $(CONFIG_STAMP) generated/linker.cmd
	@echo Linking $@
	$(LNK) $(LFLAGS) $(LIB_PATHS) -Wl,-m=$(MAP) -o $@ $(OBJS) $(LIBS) generated/linker.cmd

dirs:
	@mkdir -p $(OBJDIR) $(PQC_DIRS)

# ---- SysConfig -----------------------------------------------------------
SYSCFG_SDKPRODUCT := $(MCU_PLUS_SDK_PATH)/.metadata/product.json
syscfg generated/linker.cmd generated/ti_drivers_config.h: example.syscfg
	@echo Generating SysConfig files
	$(SYSCFG_CLI) --product $(SYSCFG_SDKPRODUCT) --context r5fss0-0 --part ALX --package ALX --output generated/ example.syscfg

# ---- ROM-bootable SBL image (HS-FS auto-signed, degenerate key) ----------
# Different signing pipeline from boot/'s bootimage rule (genimage_am64x.py,
# for a multicore-ELF APPLICATION an SBL loads) - this SBL itself needs the
# ROM Boot Loader's own image format, produced by rom_image_gen.py.
SIGNING_TOOL   := $(MCU_PLUS_SDK_PATH)/source/security/security_common/tools/boot/signing
ROM_DEGEN_KEY  := $(SIGNING_TOOL)/rom_degenerateKey.pem
SYSFW_PATH     := $(MCU_PLUS_SDK_PATH)/source/drivers/sciclient/soc/am64x_am243x
SYSFW_LOAD_ADDR    := 0x44000
BOARDCFG_LOAD_ADDR := 0x7B000
BOARDCFG_BLOB  := $(MCU_PLUS_SDK_PATH)/source/drivers/sciclient/sciclient_default_boardcfg/am243x/boardcfg_blob.bin
SBL_RUN_ADDRESS := 0x70000000

bootimage: $(NAME).$(PROFILE).hs_fs.tiimage
$(NAME).$(PROFILE).bin: $(OUT)
	$(OBJCOPY) --strip-all -O binary $(OUT) $@
$(NAME).$(PROFILE).hs_fs.tiimage: $(NAME).$(PROFILE).bin
	$(PYTHON) $(SIGNING_TOOL)/rom_image_gen.py --swrv 1 \
		--sbl-bin $(NAME).$(PROFILE).bin \
		--sysfw-bin $(SYSFW_PATH)/sysfw-hs-fs-enc.bin \
		--sysfw-inner-cert $(SYSFW_PATH)/sysfw-hs-fs-enc-cert.bin \
		--boardcfg-blob $(BOARDCFG_BLOB) \
		--sbl-loadaddr $(SBL_RUN_ADDRESS) \
		--sysfw-loadaddr $(SYSFW_LOAD_ADDR) \
		--bcfg-loadaddr $(BOARDCFG_LOAD_ADDR) \
		--key $(ROM_DEGEN_KEY) \
		--rom-image $@
	@echo Boot image: $@

clean:
	rm -rf obj $(NAME).*.out $(NAME).*.map $(NAME).*.bin $(NAME).*.tiimage* *.lnkxml

scrub: clean
	rm -rf generated

-include $(shell find $(OBJDIR) -name '*.d' 2>/dev/null)
