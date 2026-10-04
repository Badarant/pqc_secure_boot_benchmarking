#
# enclave - AM243x-LP / m4fss0-0 / nortos / ti-arm-clang
# M4F crypto enclave (DECISIONS.md -> D3, "inverted architecture", LOCKED):
# a SHA256/VERIFY IPC service, carrying no state
# between requests, which the R5F (`boot`) calls to drive its own orchestrated
# boot flow.
#
# Standalone makefile: does not use the MCU+ SDK per-example makefile. It
# compiles our sources + the vendored pqm4 ML-DSA m4f sources (as-is, R1) and
# links against the SDK prebuilt nortos/drivers/board libraries.
#
# Usage (see scripts/build.bat for the Windows wrapper that sets the tool paths):
#   gmake                       # release, ML-DSA-65 (SHA256 + VERIFY)
#   gmake PROFILE=debug
#   gmake PQSB_SCHEME=87
#   gmake bootimage             # also emit the signed .mcelf.hs_fs for `boot` to load
#   gmake clean
#
# Switching PROFILE/PQSB_SCHEME/STACK_PAINT never needs a `clean` first -
# each combination gets its own obj/ subdirectory, so a switch just builds
# once into a fresh cache and later switches back are instant.
#
# The enclave is app-agnostic and key-agnostic (R3): it never reads flash
# directly. All inputs (header/image/sig/pk addresses, the digest) arrive in
# the IPC request the R5F fills (../common/enclave_ipc.h) - nothing here needs
# regenerating when keys rotate or the app payload changes.
#
# Required (env or make var):
#   MCU_PLUS_SDK_PATH          root of the AM243x MCU+ SDK
#   CGT_TI_ARM_CLANG_PATH      TI ARM Clang (LLVM) toolchain root
#   CGT_GCC_ARM_PATH           arm-none-eabi toolchain root (assembles pqm4 .S)
#   SYSCFG_CLI                 node + sysconfig cli.js  (only for `gmake syscfg`)
#

# PROFILE: release | debug   PQSB_SCHEME: 44 | 65 | 87
PROFILE          ?= release
PQSB_SCHEME      ?= 65
# STACK_PAINT=1 measures the M4F's own .stack high-water mark and returns it to
# the R5F in enclave_resp_t.stack_peak (see ../common/stack_paint.h). `boot` must
# be built with the same setting, or it will print a stale/zero number.
STACK_PAINT      ?= 0
NAME             := enclave

# GNU Make's default goal (what a bare `gmake` builds) is "the first target in
# the file" unless pinned - fixed explicitly so an earlier-appearing helper
# rule (e.g. the FORCE/CONFIG_STAMP plumbing below) can never silently become
# it and turn a plain `gmake`/`gmake PQSB_SCHEME=NN` into a no-op.
.DEFAULT_GOAL := all

REPO_ROOT    := $(abspath ../../..)
PQM4         := $(REPO_ROOT)/third_party_sw/pqm4
COMMON       := $(REPO_ROOT)/common
BOARD_COMMON := $(abspath ../common)
# canonical pqm4 ML-DSA m4f sources (65/87 dirs are symlinks to this on POSIX)
M4F          := $(PQM4)/crypto_sign/ml-dsa-44/m4f

CC      := $(CGT_TI_ARM_CLANG_PATH)/bin/tiarmclang
LNK     := $(CGT_TI_ARM_CLANG_PATH)/bin/tiarmclang
GAS     := $(CGT_GCC_ARM_PATH)/bin/arm-none-eabi-gcc
GOBJCOPY := $(CGT_GCC_ARM_PATH)/bin/arm-none-eabi-objcopy

# ---- scheme select -> our config.h shim drives DILITHIUM_MODE ---------------
ifeq ($(PQSB_SCHEME),44)
else ifeq ($(PQSB_SCHEME),65)
else ifeq ($(PQSB_SCHEME),87)
else
  $(error PQSB_SCHEME must be 44, 65 or 87)
endif

# OBJDIR is keyed on every macro that changes what gets compiled into an object
# (PROFILE, PQSB_SCHEME, STACK_PAINT). This is not cosmetic: PQSB_SCHEME
# selects DILITHIUM_MODE via -include'd pqm4_glue/config.h, a command-line
# value with no file for make's mtime tracking to see change. Without a
# scheme-keyed OBJDIR, switching PQSB_SCHEME without `gmake clean` silently
# RELINKS STALE pqm4 crypto objects built for the previous scheme -
# enclave.o/main.o rebuild correctly (they have explicit header deps below)
# but sign.o/poly.o/etc. do not, so the enclave reports the new scheme yet
# verify fails near-instantly (CRYPTO_BYTES mismatch inside the stale
# object). Keying OBJDIR makes every combination its own cache, so a switch
# just builds (once) into a fresh dir.
CONFIG := $(PROFILE)-$(PQSB_SCHEME)
ifeq ($(STACK_PAINT),1)
  CONFIG := $(CONFIG)-paint
endif
OBJDIR := obj/$(CONFIG)
OUT    := $(NAME).$(PROFILE).out
MAP    := $(NAME).$(PROFILE).map

# $(OUT)/$(MAP) are named by PROFILE only, so unlike the scheme-keyed OBJDIR
# above, a plain mtime check can't tell $(OUT) was last LINKED for a different
# CONFIG (e.g. `gmake PQSB_SCHEME=87` after `PQSB_SCHEME=65`: the scheme-87
# objects are freshly built and newer than the old $(OUT), so it relinks and
# now holds scheme 87 - but a later `gmake bootimage` with no PQSB_SCHEME
# defaults back to 65 and reuses the *already-newer* scheme-87 $(OUT)
# unrelinked, since the scheme-65 objects themselves haven't changed since
# their own last build). Silent wrong-scheme .out/.mcelf.hs_fs, only caught
# later on-target as "algo mismatch". CONFIG_STAMP's recipe always runs (FORCE)
# but only touches the file - and so only bumps its mtime - when CONFIG
# actually differs from what's recorded, so $(OUT) relinks exactly when it
# needs to and no more.
CONFIG_STAMP := obj/.config-$(PROFILE).stamp
.PHONY: FORCE
FORCE:
$(CONFIG_STAMP): FORCE | dirs
	@if [ ! -f $@ ] || [ "$$(cat $@ 2>/dev/null)" != "$(CONFIG)" ]; then \
		echo "$(CONFIG)" > $@; \
	fi

ARCH_FLAGS := -mcpu=cortex-m4 -mfloat-abi=hard -mthumb -mfpu=fpv4-sp-d16

# ---- include paths --------------------------------------------------------
INCLUDES := \
	-I. \
	-Igenerated \
	-I$(COMMON) \
	-I$(BOARD_COMMON) \
	-Ipqm4_glue \
	-I$(M4F) \
	-I$(PQM4)/mupq/common \
	-I$(PQM4)/common \
	-I$(CGT_TI_ARM_CLANG_PATH)/include/c \
	-I$(MCU_PLUS_SDK_PATH)/source

DEFINES := -DSOC_AM243X -DOS_NORTOS -DPQSB_SCHEME=$(PQSB_SCHEME) \
           -DPQSB_STACK_PAINT=$(STACK_PAINT)

# -include our config.h shim: pqm4's params.h does #include "config.h", which
# quote-resolves to the sibling file in third_party_sw/ (hard-coded mode) before
# any -I path. Force-including ours first sets CONFIG_H + DILITHIUM_MODE so the
# upstream file no-ops - configuration only, third_party_sw/ untouched (R1).
CFLAGS_COMMON := $(ARCH_FLAGS) -g -ffunction-sections -fdata-sections -std=c11 \
                 -include pqm4_glue/config.h
ifeq ($(PROFILE),debug)
  CFLAGS_COMMON += -D_DEBUG_=1 -O0
else
  CFLAGS_COMMON += -Os
endif

# our code: strict, but do not police the SDK headers it pulls in
CFLAGS_APP  := $(CFLAGS_COMMON) -Wall -Wextra -Wno-unused-parameter
# vendored pqm4: as-is, tolerate its warnings (never edit upstream, R1).
# The trailing -O2 overrides the profile's own -Os (last flag wins): this is the
# code the benchmark measures. It is FIXED, not a build parameter - it is the
# level boards/nucleo_L4R5ZI uses, and holding both boards at one level is what
# makes their figures comparable. boot/makefile fixes the same level; the two
# must not drift apart. Note most of pqm4's ML-DSA speed lives in hand-written
# .S files, which no -O level touches - only the C glue moves (~4 KB of ROM).
CFLAGS_PQM4 := $(CFLAGS_COMMON) -Wno-constant-conversion -Wno-implicit-int \
               -Wno-unused-function -Wno-unused-but-set-variable -Wno-sign-compare \
               -O2
# pqm4's sha2.c (the SHA256 op) builds with these same flags - see its rule
# below - so it is covered too.

# ---- sources ------------------------------------------------------------
APP_C := main.c enclave.c
GLUE_C := pqm4_glue/pqsb_randombytes_stub.c

SYSCFG_C := \
	generated/ti_drivers_config.c \
	generated/ti_drivers_open_close.c \
	generated/ti_board_config.c \
	generated/ti_board_open_close.c \
	generated/ti_dpl_config.c \
	generated/ti_pinmux_config.c \
	generated/ti_power_clock_config.c

# includes mupq/common/sha2.c - the SHA-256 used for enclave_sha256().
# sha2.c's unused SHA-384/512 paths pull in crypto_hashblocks_sha512() (a
# separate pqm4 file, not compiled here); TI's linker drops that dead code by
# default (--unused_section_elimination=on, see LFLAGS below), so it never
# needs linking.
PQM4_C := \
	$(M4F)/packing.c \
	$(M4F)/poly.c \
	$(M4F)/polyvec.c \
	$(M4F)/rounding.c \
	$(M4F)/sign.c \
	$(M4F)/smallpoly.c \
	$(M4F)/symmetric-shake.c \
	$(PQM4)/mupq/common/fips202.c \
	$(PQM4)/mupq/common/sha2.c

# pqm4 hand-written asm - GNU-as syntax, assembled with arm-none-eabi-gcc
# (unmodified input; TI ARM Clang's assembler rejects the =#literal / vmov.w
# forms these files use). This is the sanctioned fallback in P1_boot_spec.md.
PQM4_S := \
	$(M4F)/ntt.S \
	$(M4F)/smallntt_769.S \
	$(M4F)/pointwise_mont.s \
	$(M4F)/vector.s \
	$(PQM4)/common/keccakf1600.S

OBJS := \
	$(addprefix $(OBJDIR)/app/,$(APP_C:.c=.o)) \
	$(OBJDIR)/glue/pqsb_randombytes_stub.o \
	$(addprefix $(OBJDIR)/syscfg/,$(notdir $(SYSCFG_C:.c=.o))) \
	$(addprefix $(OBJDIR)/pqm4/,$(notdir $(PQM4_C:.c=.o))) \
	$(addprefix $(OBJDIR)/pqm4/,$(addsuffix .o,$(notdir $(PQM4_S))))

# ---- SDK link inputs --------------------------------------------------------
LIB_PATHS := \
	-Wl,-i$(MCU_PLUS_SDK_PATH)/source/kernel/nortos/lib \
	-Wl,-i$(MCU_PLUS_SDK_PATH)/source/drivers/lib \
	-Wl,-i$(MCU_PLUS_SDK_PATH)/source/board/lib \
	-Wl,-i$(CGT_TI_ARM_CLANG_PATH)/lib

LIBS := \
	-lnortos.am243x.m4f.ti-arm-clang.$(PROFILE).lib \
	-ldrivers.am243x.m4f.ti-arm-clang.nortos.$(PROFILE).lib \
	-lboard.am243x.m4f.ti-arm-clang.nortos.$(PROFILE).lib \
	-llibc.a \
	-llibsysbm.a

# TI's linker (tiarmlnk) is not GNU ld - it has no --gc-sections flag (that
# option name is invalid here and fails the link). Its own dead-code removal,
# --unused_section_elimination, defaults to on, so pqm4 mupq/common/sha2.c's
# unused SHA-384/512 paths (which call into a pqm4 file we do not compile, see
# PQM4_C) are dropped automatically; -ffunction-sections/-fdata-sections above
# just make that elimination finer-grained. generated/linker.cmd already
# --retain="*(.vectors)".
LFLAGS := $(ARCH_FLAGS) -Wl,--ram_model -Wl,--reread_libs \
          -Wl,--xml_link_info=$(NAME).$(PROFILE).lnkxml

# ==========================================================================
.PHONY: all clean scrub syscfg bootimage
all: $(OUT)

# every object waits for SysConfig to have generated its headers (order-only:
# regenerating syscfg does not by itself force a full object rebuild)
# TI's -MMD writes dependency paths with WINDOWS BACKSLASHES
# (C:\ti\...\DebugP.h). GNU Make treats backslash as an escape character in a
# prerequisite list, so it never matches the real file and the generated .d
# files silently track NOTHING. Proven by touching ../common/enclave_ipc.h and
# watching make rebuild zero objects - which is exactly how a stale enclave
# kept using the old IPC mailbox address after the MSRAM re-layout, and cost a
# hardware round trip to find. Until the .d files are post-processed, depend on
# the shared and local headers explicitly. Coarse (one header touched rebuilds
# every object) but correct, and these are small projects.
TRACKED_H := $(wildcard $(COMMON)/*.h) $(wildcard $(BOARD_COMMON)/*.h) $(wildcard *.h)
# The makefile itself is a prerequisite too: now that -O2 is hard-coded rather
# than a variable in the OBJDIR key, editing it is the ONLY way to change the
# crypto optimisation level, and make would otherwise not notice. Same for any
# other flag edit here.
$(OBJS): $(TRACKED_H) makefile

$(OBJS): | generated/ti_drivers_config.h

$(OBJDIR)/app/enclave.o: enclave.h
$(OBJDIR)/app/main.o: enclave.h

$(OUT): $(OBJS) $(CONFIG_STAMP) generated/linker.cmd
	@echo Linking $@
	$(LNK) $(LFLAGS) $(LIB_PATHS) -Wl,-m=$(MAP) -o $@ $(OBJS) $(LIBS) generated/linker.cmd

# --- our code
$(OBJDIR)/app/%.o: %.c | dirs
	$(CC) -c $(CFLAGS_APP) $(INCLUDES) $(DEFINES) -MMD -o $@ $<

$(OBJDIR)/glue/pqsb_randombytes_stub.o: pqm4_glue/pqsb_randombytes_stub.c | dirs
	$(CC) -c $(CFLAGS_APP) $(INCLUDES) $(DEFINES) -MMD -o $@ $<

# --- syscfg generated code (SDK style, tolerate its warnings)
$(OBJDIR)/syscfg/%.o: generated/%.c | dirs
	$(CC) -c $(CFLAGS_COMMON) -Wno-unused-function $(INCLUDES) $(DEFINES) -MMD -o $@ $<

# --- vendored pqm4 C (as-is)
$(OBJDIR)/pqm4/%.o: $(M4F)/%.c | dirs
	$(CC) -c $(CFLAGS_PQM4) $(INCLUDES) $(DEFINES) -MMD -o $@ $<

$(OBJDIR)/pqm4/fips202.o: $(PQM4)/mupq/common/fips202.c | dirs
	$(CC) -c $(CFLAGS_PQM4) $(INCLUDES) $(DEFINES) -MMD -o $@ $<

$(OBJDIR)/pqm4/sha2.o: $(PQM4)/mupq/common/sha2.c | dirs
	$(CC) -c $(CFLAGS_PQM4) $(INCLUDES) $(DEFINES) -MMD -o $@ $<

# --- vendored pqm4 asm (GNU assembler, unmodified input)
# TI ARM Clang's assembler rejects the =#literal / vmov.w forms these files use,
# so they are assembled with arm-none-eabi-gcc (sanctioned fallback, P1 spec 5b).
# The .ARM.attributes section gcc stamps (Tag_ABI_VFP_args) is then stripped so
# TI's linker accepts the objects: the routines take only integer/pointer args
# in core registers, so the VFP-args tag is meaningless for them.
ASM_STRIP = $(GOBJCOPY) -R .ARM.attributes $@ $@

$(OBJDIR)/pqm4/%.S.o: $(M4F)/%.S | dirs
	$(GAS) -c $(ARCH_FLAGS) -I$(M4F) -I$(PQM4)/common -o $@ $<
	$(ASM_STRIP)

$(OBJDIR)/pqm4/%.s.o: $(M4F)/%.s | dirs
	$(GAS) -c $(ARCH_FLAGS) -I$(M4F) -I$(PQM4)/common -o $@ $<
	$(ASM_STRIP)

$(OBJDIR)/pqm4/keccakf1600.S.o: $(PQM4)/common/keccakf1600.S | dirs
	$(GAS) -c $(ARCH_FLAGS) -I$(M4F) -I$(PQM4)/common -o $@ $<
	$(ASM_STRIP)

dirs:
	@mkdir -p $(OBJDIR)/app $(OBJDIR)/glue $(OBJDIR)/syscfg $(OBJDIR)/pqm4

# ---- SysConfig -----------------------------------------------------------
SYSCFG_SDKPRODUCT := $(MCU_PLUS_SDK_PATH)/.metadata/product.json
syscfg generated/linker.cmd generated/ti_drivers_config.h: example.syscfg
	@echo Generating SysConfig files
	$(SYSCFG_CLI) --product $(SYSCFG_SDKPRODUCT) --context m4fss0-0 --part ALX --package ALX --output generated/ example.syscfg

# ---- SBL-loadable multicore-ELF boot image (HS-FS auto-signed) -------------
MCELF_GEN     := $(MCU_PLUS_SDK_PATH)/tools/boot/multicore-elf/genimage_am64x.py
APP_SIGN      := $(MCU_PLUS_SDK_PATH)/source/security/security_common/tools/boot/signing/appimage_x509_cert_gen.py
APP_SIGN_KEY  := $(MCU_PLUS_SDK_PATH)/source/security/security_common/tools/boot/signing/app_degenerateKey.pem
CORE_ID_M4F   := 14

# "python" resolved via PATH can hit a Cygwin symlink stub that Windows'
# CreateProcess cannot execute ("make (e=5): Access is denied" from gmake).
# scripts/build.bat resolves a real python.exe and passes PYTHON= on the
# command line; this default only matters for a bare `gmake bootimage`.
PYTHON ?= python

bootimage: $(NAME).$(PROFILE).mcelf.hs_fs
$(NAME).$(PROFILE).mcelf.hs_fs: $(OUT)
	$(PYTHON) $(MCELF_GEN) --core-img=$(CORE_ID_M4F):$(OUT) --output=$(NAME).$(PROFILE).mcelf
	$(PYTHON) $(APP_SIGN) --bin $(NAME).$(PROFILE).mcelf --authtype 1 --key $(APP_SIGN_KEY) --output $@
	@echo Boot image: $@

clean:
	rm -rf obj $(NAME).*.out $(NAME).*.map $(NAME).*.mcelf* *.lnkxml

scrub: clean
	rm -rf generated

-include $(shell find $(OBJDIR) -name '*.d' 2>/dev/null)
