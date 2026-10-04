@echo off
rem ===========================================================================
rem env.bat - tool locations and defaults for everything under
rem boards/LP_AM243: the R5F `boot` orchestrator, the M4F `enclave`, and the
rem build/load/flash/JTAG scripts of both.
rem
rem One file for the whole board, called as ..\..\env.bat from
rem <project>\scripts\*.bat.
rem
rem Edit these to match your install, or pre-set them in the environment: every
rem value is guarded with `if not defined`, so whatever is already set wins.
rem ===========================================================================

if not defined MCU_PLUS_SDK_PATH      set "MCU_PLUS_SDK_PATH=C:\ti\mcu_plus_sdk_am243x_12_00_00_27"
if not defined CGT_TI_ARM_CLANG_PATH  set "CGT_TI_ARM_CLANG_PATH=C:\ti\ti_cgt_arm_llvm_4.0.4.LTS"

rem GNU Arm Embedded is needed by `enclave` ONLY, to assemble pqm4's
rem hand-written Cortex-M4 .S files. `boot` never touches it.
if not defined CGT_GCC_ARM_PATH       set "CGT_GCC_ARM_PATH=C:\arm-none-eabi"

rem SysConfig 1.26.0 for both projects. This is the release the SDK's own
rem imports.mak pins for 12.00.00.27, and it is a hard requirement for `boot`:
rem its example.syscfg derives from the stock sbl_ospi example, and that file
rem CRASHES under 1.28.0 - "Cannot read properties of undefined (reading
rem 'configurables')" while resolving OSPI pin requirements inside
rem /board/flash/flash's addInstance(). That is a version-skew bug in the OSPI
rem pinmux module metadata, not in our configuration: it reproduces against a
rem byte-for-byte stock copy of the file, and 1.26.0 processes it cleanly.
rem
rem `enclave`'s example.syscfg was originally authored under 1.28.0, but it
rem carries no OSPI/flash module and 1.26.0 generates it identically -
rem all 26 generated files compare byte-for-byte equal between the two
rem releases. So one version serves both projects, and only 1.26.0 need be
rem installed.
if not defined SYSCONFIG_PATH         set "SYSCONFIG_PATH=C:\ti\sysconfig_1.26.0"

rem Only gmake is used from CCS.
rem Note CCS ships its own TI ARM Clang (21.0.0 bundles 5.1.1.LTS); we
rem deliberately point CGT_TI_ARM_CLANG_PATH at the standalone 4.0.4.LTS
rem instead, because that is the compiler every measurement in this repo was
rem taken with.
if not defined CCS_PATH               set "CCS_PATH=C:\ti\ccs2100\ccs"

if not defined GMAKE                  set "GMAKE=%CCS_PATH%\utils\bin\gmake.exe"
if not defined SYSCFG_NODE            set "SYSCFG_NODE=%SYSCONFIG_PATH%\nodejs\node.exe"
if not defined SYSCFG_CLI_JS          set "SYSCFG_CLI_JS=%SYSCONFIG_PATH%\dist\cli.js"
if not defined PYTHON                 set "PYTHON=python"

rem --- build defaults (override on the command line: build.bat PROFILE=debug) --
if not defined PROFILE      set "PROFILE=release"
if not defined PQSB_SCHEME  set "PQSB_SCHEME=87"

rem --- board connection ------------------------------------------------------
rem Only flash.bat reads this, and its first argument overrides it - so this is
rem just the fallback for `flash.bat` with no port given.
if not defined SERIAL_PORT  set "SERIAL_PORT=COM4"

set "SYSCFG_CLI=%SYSCFG_NODE% %SYSCFG_CLI_JS%"
