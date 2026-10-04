@echo off
rem ===========================================================================
rem build.bat - build the AM243x-LP `enclave` M4F crypto service.
rem
rem   build.bat                       release, ML-DSA-65 (SHA256 + VERIFY)
rem   build.bat PROFILE=debug
rem   build.bat PQSB_SCHEME=87        (re-provision keys/pubkey.bin to match: see
rem                                    tools/provisioning.py at the repo root)
rem   build.bat clean
rem   build.bat bootimage            also emit enclave.<profile>.mcelf.hs_fs
rem
rem enclave never reads flash directly - all inputs (addresses + digest) arrive
rem in the IPC request `boot` (R5F) fills, so nothing here needs regenerating
rem when the app payload or the key rotates; only the scheme (PQSB_SCHEME) has
rem to match provisioning.
rem
rem Output: enclave.<PROFILE>.out  (load with scripts\run.bat)
rem ===========================================================================
setlocal
call "%~dp0..\..\env.bat"
cd /d "%~dp0.."

echo.
echo === enclave build : PROFILE=%PROFILE% PQSB_SCHEME=%PQSB_SCHEME% ===
echo   SDK  : %MCU_PLUS_SDK_PATH%
echo   CGT  : %CGT_TI_ARM_CLANG_PATH%
echo   GCC  : %CGT_GCC_ARM_PATH%   (assembles the pqm4 hand-written .S)
echo.

if not exist "%CGT_TI_ARM_CLANG_PATH%\bin\tiarmclang.exe" (
    echo [ERROR] tiarmclang not found - fix CGT_TI_ARM_CLANG_PATH in ..\..\env.bat
    exit /b 2
)
if not exist "%CGT_GCC_ARM_PATH%\bin\arm-none-eabi-gcc.exe" (
    echo [ERROR] arm-none-eabi-gcc not found - fix CGT_GCC_ARM_PATH in ..\..\env.bat
    echo         ^(pqm4's Keccak/NTT .S are GNU-as syntax; TI's assembler rejects them^)
    exit /b 2
)

rem "python" on PATH can resolve to a Cygwin symlink stub that Windows'
rem CreateProcess cannot execute ("make (e=5): Access is denied" - hits the
rem `bootimage` target's genimage_am64x.py/appimage_x509_cert_gen.py steps).
rem Resolve a real python.exe via the py launcher and pass it through.
set PYTHON=
for /f "delims=" %%p in ('py -3 -c "import sys; print(sys.executable)" 2^>nul') do set PYTHON=%%p
if not defined PYTHON set PYTHON=python

"%GMAKE%" MCU_PLUS_SDK_PATH="%MCU_PLUS_SDK_PATH%" ^
          CGT_TI_ARM_CLANG_PATH="%CGT_TI_ARM_CLANG_PATH%" ^
          CGT_GCC_ARM_PATH="%CGT_GCC_ARM_PATH%" ^
          SYSCFG_CLI="%SYSCFG_CLI%" ^
          PROFILE=%PROFILE% PQSB_SCHEME=%PQSB_SCHEME% PYTHON="%PYTHON%" ^
          %*
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
    echo === build OK : %CD%\enclave.%PROFILE%.out ===
) else (
    echo === build FAILED ^(rc=%RC%^) ===
)
exit /b %RC%
