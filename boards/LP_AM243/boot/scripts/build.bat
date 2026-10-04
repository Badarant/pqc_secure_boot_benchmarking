@echo off
rem ===========================================================================
rem build.bat - build the AM243x-LP custom SBL (boot, R5F0-0).
rem
rem   build.bat                       release
rem   build.bat PROFILE=debug
rem   build.bat clean
rem   build.bat bootimage            also emit the ROM-bootable .hs_fs.tiimage
rem
rem Output: boot.<PROFILE>.out  /  ...hs_fs.tiimage (for flash.bat)
rem ===========================================================================
setlocal
call "%~dp0..\..\env.bat"
cd /d "%~dp0.."

echo.
echo === boot build : PROFILE=%PROFILE% ===
echo   SDK  : %MCU_PLUS_SDK_PATH%
echo   CGT  : %CGT_TI_ARM_CLANG_PATH%
echo.

if not exist "%CGT_TI_ARM_CLANG_PATH%\bin\tiarmclang.exe" (
    echo [ERROR] tiarmclang not found - fix CGT_TI_ARM_CLANG_PATH in ..\..\env.bat
    exit /b 2
)

rem "python" on PATH can resolve to a Cygwin symlink stub that Windows'
rem CreateProcess cannot execute (see boards/LP_AM243/boot/scripts/build.bat).
set PYTHON=
for /f "delims=" %%p in ('py -3 -c "import sys; print(sys.executable)" 2^>nul') do set PYTHON=%%p
if not defined PYTHON set PYTHON=python

"%GMAKE%" MCU_PLUS_SDK_PATH="%MCU_PLUS_SDK_PATH%" ^
          CGT_TI_ARM_CLANG_PATH="%CGT_TI_ARM_CLANG_PATH%" ^
          SYSCFG_CLI="%SYSCFG_CLI%" ^
          PROFILE=%PROFILE% PYTHON="%PYTHON%" ^
          %*
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
    echo === build OK : %CD%\boot.%PROFILE%.out ===
) else (
    echo === build FAILED ^(rc=%RC%^) ===
)
exit /b %RC%
