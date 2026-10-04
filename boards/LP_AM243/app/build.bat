@echo off
rem ===========================================================================
rem build.bat - build the AM243x-LP test application (bare-metal Cortex-R5F).
rem Output: out\app.bin  (raw MSRAM image, load at BOOT_LOAD_ADDR - see linker.ld)
rem
rem   build.bat
rem   build.bat clean
rem ===========================================================================
setlocal
cd /d "%~dp0"

if not defined CGT_GCC_ARM_PATH set "CGT_GCC_ARM_PATH=C:\arm-none-eabi"
if not defined GMAKE           set "GMAKE=C:\ti\ccs2100\ccs\utils\bin\gmake.exe"

if not exist "%CGT_GCC_ARM_PATH%\bin\arm-none-eabi-gcc.exe" (
    echo [ERROR] arm-none-eabi-gcc not found - set CGT_GCC_ARM_PATH
    exit /b 2
)

"%GMAKE%" CGT_GCC_ARM_PATH="%CGT_GCC_ARM_PATH%" %*
set "RC=%ERRORLEVEL%"
if "%RC%"=="0" ( echo === app build OK : %CD%\out\app.bin === ) else ( echo === app build FAILED ^(rc=%RC%^) === )
exit /b %RC%
