@echo off
rem ===========================================================================
rem flash.bat - program the AM243x-LP OSPI flash over UART (ROM UART boot mode +
rem             TI's uart_uniflash). Set the boot-mode switches to UART boot,
rem             connect the XDS110 USB cable, then run this.
rem
rem `boot` (R5F, this project) IS the SBL now (DECISIONS.md -> D3, "inverted
rem architecture") - built on the SDK's own sbl_ospi foundation, it does full
rem SYSFW/board-config init itself when ROM loads it from OSPI 0x0. There is
rem no separate SBL wrapper to flash first and no JTAG bring-up step needed:
rem flashing the bundle below is the complete, self-sufficient boot chain.
rem
rem   flash.bat COM7                flash out\signed_app.bin -> OSPI 0x300000
rem                                 (the application; update independently of
rem                                 boot+enclave+pubkey any time)
rem   flash.bat COM7 --pubkey       flash keys\pubkey.bin -> OSPI 0x200000
rem                                 (root-of-trust key rotation; no rebuild)
rem   flash.bat COM7 --erase        erase the whole flash first
rem   flash.bat COM7 --offset 0x400000
rem   flash.bat COM7 --with-sbl     flash the whole bundle: `boot` itself @ 0x0
rem                                 (the SBL) + `enclave`'s multicore image @
rem                                 0x80000 + pubkey.bin @ 0x200000 (needs
rem                                 `build.bat bootimage` in BOTH boot/ and
rem                                 ../enclave/; see README)
rem   flash.bat COM7 --code-only    flash `boot` @ 0x0 + `enclave` @ 0x80000,
rem                                 skipping pubkey.bin and signed_app.bin.
rem                                 Use this when a change touches both cores
rem                                 (e.g. the enclave_ipc.h contract) but the
rem                                 key and the app are unchanged.
rem   flash.bat COM7 --boot-only    flash ONLY `boot` @ 0x0 and nothing else.
rem                                 For iterating on the SBL: signed_app.bin can
rem                                 be ~900 KiB and takes far longer to send
rem                                 than boot does, so skip it when it has not
rem                                 changed. Everything else stays as flashed.
rem
rem NOTE for BOOT_CONFIG=B: that build never loads or starts the M4F, so the
rem enclave image at 0x80000 is not needed at all - `--boot-only` is the whole
rem update. An enclave left over from a config-A flash is simply ignored, so
rem switching A -> B needs no erase. Switching B -> A does need the enclave
rem present: use `--code-only` (or `--with-sbl` on a fresh part).
rem
rem out\signed_app.bin and keys\pubkey.bin are at the REPO ROOT (both gitignored)
rem - provision them first:
rem   python ..\..\..\..\tools\provisioning.py --scheme 65 --app ..\app\out\app.bin
rem (or, from the repo root:  python tools\provisioning.py --scheme 65 ...)
rem ===========================================================================
setlocal EnableDelayedExpansion
set "SCRIPTDIR=%~dp0"
cd /d "%SCRIPTDIR%.."
for %%I in ("%SCRIPTDIR%..\..\..\..") do set "REPOROOT=%%~fI"

set "WITH_SBL="
set "DO_ERASE="
set "PUBKEY_ONLY="
set "BOOT_ONLY="
set "CODE_ONLY="
if not "%~1"=="" set "SERIAL_PORT=%~1"
:argloop
shift
if "%~1"=="" goto argsdone
if /I "%~1"=="--with-sbl"  set "WITH_SBL=1"
if /I "%~1"=="--boot-only" set "BOOT_ONLY=1"
if /I "%~1"=="--code-only" set "CODE_ONLY=1"
if /I "%~1"=="--erase"     set "DO_ERASE=1"
if /I "%~1"=="--pubkey"    set "PUBKEY_ONLY=1"
if /I "%~1"=="--offset"    ( set "SLOT_OFFSET=%~2" & shift )
goto argloop
:argsdone

call "%SCRIPTDIR%..\..\env.bat"

if not defined SLOT_OFFSET set "SLOT_OFFSET=0x300000"

rem uart_uniflash parses the cfg with POSIX shlex, which eats Windows backslashes
set "SDK=%MCU_PLUS_SDK_PATH:\=/%"
set "BOOTDIR=%CD:\=/%"
set "REPO=%REPOROOT:\=/%"
set "SBLDIR=%SDK%/tools/boot/sbl_prebuilt/am243x-lp"
set "FLASHWRITER=%SBLDIR%/sbl_uart_uniflash.release.hs_fs.tiimage"
rem `boot` itself, self-produced (scripts/build.bat bootimage) - no cross-project
rem reference needed, unlike the earlier sbl_integration/boot split.
set "SBL_BOOT=%BOOTDIR%/boot.%PROFILE%.hs_fs.tiimage"
set "PUBKEY=%REPO%/keys/pubkey.bin"
set "SIGNED_APP=%REPO%/out/signed_app.bin"
set "ENCLAVE_MCELF=%REPO%/boards/LP_AM243/enclave/enclave.%PROFILE%.mcelf.hs_fs"

if not exist "%MCU_PLUS_SDK_PATH%\tools\boot\sbl_prebuilt\am243x-lp\sbl_uart_uniflash.release.hs_fs.tiimage" (
    echo [ERROR] flashwriter not found under %MCU_PLUS_SDK_PATH% - check ..\..\env.bat
    exit /b 2
)
rem Preflight for TI's uart_uniflash.py (MCU+ SDK, tools/boot/), which this
rem script invokes to flash. These are that tool's own imports - this project's
rem Python (tools/provisioning.py, tools/py_dilithium_sign/) uses none of them.
%PYTHON% -c "import serial,tqdm,xmodem,elftools" 2>nul
if errorlevel 1 (
    echo [ERROR] TI SDK flashing tool ^(uart_uniflash.py^) is missing its Python
    echo         dependencies. These belong to the SDK tool, not to this project.
    echo         Install them into whichever interpreter runs the flashing step:
    echo             %PYTHON% -m pip install pyserial tqdm xmodem pyelftools
    exit /b 2
)

set "CFG=%TEMP%\pqsb_flash_%RANDOM%.cfg"
> "%CFG%" echo --flash-writer=%FLASHWRITER%
>>"%CFG%" echo --operation=flash-phy-tuning-data
if defined DO_ERASE >>"%CFG%" echo --file=%FLASHWRITER% --operation=erase --flash-offset=0x0 --erase-size=0x4000000

if defined BOOT_ONLY (
    if not exist "%CD%\boot.%PROFILE%.hs_fs.tiimage" (
        echo [ERROR] boot.%PROFILE%.hs_fs.tiimage not found - run:  scripts\build.bat bootimage
        exit /b 2
    )
    >>"%CFG%" echo --file=%SBL_BOOT% --operation=flash --flash-offset=0x0
    set "SUMMARY=boot @ OSPI 0x0 only - enclave, pubkey and signed_app left untouched"
) else if defined CODE_ONLY (
    if not exist "%CD%\boot.%PROFILE%.hs_fs.tiimage" (
        echo [ERROR] boot.%PROFILE%.hs_fs.tiimage not found - run:  scripts\build.bat bootimage
        exit /b 2
    )
    if not exist "%REPOROOT%\boards\LP_AM243\enclave\enclave.%PROFILE%.mcelf.hs_fs" (
        echo [ERROR] enclave.%PROFILE%.mcelf.hs_fs not found - run:
        echo         ..\..\enclave\scripts\build.bat bootimage
        exit /b 2
    )
    >>"%CFG%" echo --file=%SBL_BOOT% --operation=flash --flash-offset=0x0
    >>"%CFG%" echo --file=%ENCLAVE_MCELF% --operation=flash --flash-offset=0x80000
    set "SUMMARY=boot @ OSPI 0x0 + enclave @ 0x80000 - pubkey and signed_app left untouched"
) else if defined PUBKEY_ONLY (
    if not exist "%REPOROOT%\keys\pubkey.bin" (
        echo [ERROR] %REPOROOT%\keys\pubkey.bin not found.
        echo         Run ^(from the repo root^):  python tools\provisioning.py --scheme %PQSB_SCHEME%
        exit /b 2
    )
    >>"%CFG%" echo --file=%PUBKEY% --operation=flash --flash-offset=0x200000
    set "SUMMARY=pubkey.bin flashed @ OSPI 0x200000 (XIP 0x60200000 = BOOT_PUBKEY_BASE) - key rotated, no rebuild needed"
) else (
    if defined WITH_SBL (
        if not exist "%CD%\boot.%PROFILE%.hs_fs.tiimage" (
            echo [ERROR] boot.%PROFILE%.hs_fs.tiimage not found - run:  scripts\build.bat bootimage
            exit /b 2
        )
        if not exist "%REPOROOT%\boards\LP_AM243\enclave\enclave.%PROFILE%.mcelf.hs_fs" (
            echo [ERROR] enclave.%PROFILE%.mcelf.hs_fs not found - run:
            echo         ..\..\enclave\scripts\build.bat bootimage
            exit /b 2
        )
        if not exist "%REPOROOT%\keys\pubkey.bin" (
            echo [ERROR] %REPOROOT%\keys\pubkey.bin not found.
            echo         Run ^(from the repo root^):  python tools\provisioning.py --scheme %PQSB_SCHEME%
            exit /b 2
        )
        >>"%CFG%" echo --file=%SBL_BOOT% --operation=flash --flash-offset=0x0
        >>"%CFG%" echo --file=%ENCLAVE_MCELF% --operation=flash --flash-offset=0x80000
        >>"%CFG%" echo --file=%PUBKEY% --operation=flash --flash-offset=0x200000
    )
    if not exist "%REPOROOT%\out\signed_app.bin" (
        echo [ERROR] %REPOROOT%\out\signed_app.bin not found.
        echo         Run ^(from the repo root^):  python tools\provisioning.py --scheme %PQSB_SCHEME%
        exit /b 2
    )
    >>"%CFG%" echo --file=%SIGNED_APP% --operation=flash --flash-offset=%SLOT_OFFSET%
    set "SUMMARY=signed_app.bin @ OSPI %SLOT_OFFSET% (XIP 0x60000000 + %SLOT_OFFSET% = BOOT_IMAGE_BASE)"
)

echo.
echo === flashing over UART on %SERIAL_PORT% ===
type "%CFG%"
echo.

%PYTHON% "%MCU_PLUS_SDK_PATH%\tools\boot\uart_uniflash.py" -p %SERIAL_PORT% --cfg "%CFG%"
set "RC=%ERRORLEVEL%"
del "%CFG%" 2>nul

echo.
if "%RC%"=="0" (
    echo === flash OK ===
    echo   !SUMMARY!
) else (
    echo === flash FAILED ^(rc=%RC%^) - check the COM port and that the board is in UART boot mode ===
)
exit /b %RC%
