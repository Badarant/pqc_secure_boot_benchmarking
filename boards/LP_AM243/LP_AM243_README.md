# boards/LP_AM243 PQC secure boot

Post-quantum secure boot for the TI [**AM243x LaunchPad™ (LP-AM243)**](https://www.ti.com/tool/LP-AM243), built to be
measured. The **R5F** drives the whole boot flow; `boot` authenticates an opaque
application image at a fixed address with SHA2-256 + ML-DSA and, on
success, releases it.

**Two configurations are built, and the comparison between them is the point:**

| | SHA-256 | ML-DSA verify | IPC |
|---|---|---|---|
| **A** | software, on the M4F enclave | pqm4 m4f (Cortex-M4 assembly), on the M4F enclave | yes, both steps |
| **B** | software, on the R5F (same pqm4 `sha2.c`) | PQClean `clean` (portable C), on the R5F | **none**, enclave never started |

ML-DSA comes from the shared `third_party_sw/pqm4`. m4fspeed for the M4F,
PQClean's portable C for the R5F, from:

https://github.com/mupq/pqm4

https://github.com/mupq/mupq/tree/ddcccedb2db9d0250856bd58ee5c46c61e506c5d

https://github.com/pqclean/pqclean/tree/c3e6861fbb0a0b2721d2599c0a68430061414f18

## Architecture

1. ROM boots `boot/` directly (R5F0-0). It is the SBL (built on the SDK's
   own `sbl_ospi` example), not a separate wrapper around one.
2. **Config A only:** `boot` loads + starts `enclave/` (M4F0-0), a crypto
   service that holds no state between requests, and sets up IPC Notify with
   it (`common/enclave_ipc.h`). **Config B** skips this entirely. 
3. `boot` reads `signed_app.bin` from OSPI and obtains a digest and a verdict:
   in config A by two `SHA256` requests and one `VERIFY` request to the enclave;
   in config B by computing both locally. The pubkey is read from flash by
   `boot` in either case, never compiled in.
4. Before the image is trusted, `boot` hashes the **public key** and compares it
   to a stand-in for the device's root-of-trust fuse. Advisory only on this
   HS-FS part, and deliberately not acted on. But this is measured, so the benchmark
   reports what a complete chain of trust costs rather than what a shortcut
   costs.
5. `boot` enforces the verdict itself: on pass it jumps to the verified image in
   MSRAM; on fail it halts. No IPC "go" signal is needed for the hand-off. The
   R5F already has the verdict, in both configurations.


## Installing

Windows host. Toolchain:

These are the versions this project is built and verified with, and the default
paths the scripts expect:

| Component | Version used | Default path | Needed by |
|---|---|---|---|
| Code Composer Studio | 21.0.0 | `C:\ti\ccs2100\ccs` | `gmake`, used by both builds |
| MCU+ SDK for AM243x | 12.00.00.27 | `C:\ti\mcu_plus_sdk_am243x_12_00_00_27` | drivers/board libraries, image signing, flashing |
| TI ARM Clang | 4.0.4.LTS | `C:\ti\ti_cgt_arm_llvm_4.0.4.LTS` | compiles both `boot` and `enclave` |
| SysConfig | 1.26.0 | `C:\ti\sysconfig_1.26.0` | generates both projects' drivers/linker config. this exact release |
| GNU Arm Embedded Toolchain | 10.3-2021.10 | `C:\arm-none-eabi` | `enclave` only: assembles pqm4's hand-written `.S` files |
| Python 3 | 3.13 | on `PATH` | host signer, flashing tool, SDK image-signing scripts |

Download from TI:

- CCS: <https://www.ti.com/tool/CCSTUDIO>
- MCU+ SDK AM243x: <https://www.ti.com/tool/MCU-PLUS-SDK-AM243X>
- TI ARM Clang and SysConfig: <https://www.ti.com/tool/ARM-CGT> and
  <https://www.ti.com/tool/SYSCONFIG> (both also installable through the CCS
  installer's "Add-ons")

GNU Arm Embedded 10.3-2021.10 comes from Arm's GNU toolchain downloads page
(<https://developer.arm.com/downloads/-/gnu-rm>, listed under the older
releases). Extract it so that `arm-none-eabi-gcc.exe` ends up in
`C:\arm-none-eabi\bin\`.

**Do not substitute the TI ARM Clang that ships inside CCS.** CCS 21.0.0 bundles
`ti-cgt-armllvm_5.1.1.LTS`; this project is verified against the standalone
**4.0.4.LTS**, which is why `env.bat` points outside the CCS tree. 5.1.1
may well work, but no measurement in this repo was taken with it, and the
benchmark figures are only comparable across builds made with one compiler.


### Python packages

Two independent sets. The host signer:

```
py -3 -m pip install dilithium-py
```

and TI's flashing tool (`uart_uniflash.py`, from the SDK), which this project
only drives:

```
py -3 -m pip install pyserial tqdm xmodem pyelftools
```

`flash.bat` checks for the second set up front and tells you this command if any
of it is missing, rather than failing inside TI's script. Nothing in
`tools/provisioning.py` or `tools/py_dilithium_sign/` uses those four.

If `python` on your `PATH` resolves to a Cygwin/MSYS or Microsoft Store stub,
`CreateProcess` cannot execute it and the SDK's signing scripts fail with no
useful message. The build scripts work around this by resolving a real
interpreter through `py -3` themselves; if you override `PYTHON` in `env.bat`,
point it at a real `python.exe`.

### Pointing the scripts at your install

Everything above is read from a single file, **`boards\LP_AM243\env.bat`**,
shared by `boot`, `enclave` and all the build/flash/JTAG scripts (each calls it
as `..\..\env.bat`). Either edit the paths there, or pre-set any of them in the
environment before calling a script. Every value is guarded with
`if not defined`, so the environment wins.

### Verifying the setup

```
C:\ti\ti_cgt_arm_llvm_4.0.4.LTS\bin\tiarmclang --version
C:\arm-none-eabi\bin\arm-none-eabi-gcc --version
C:\ti\ccs2100\ccs\utils\bin\gmake.exe --version
py -3 -c "import dilithium_py, serial, tqdm, xmodem, elftools; print('host deps OK')"
```

## Building

The M4F enclave

```
.\enclave\scripts\build.bat bootimage PQSB_SCHEME=scheme
```

where scheme is 44, 65 or 87

The R5F boot

```
.\boot\scripts\build.bat bootimage BOOT_CONFIG=config PQC_SCHEME=scheme
```

where config is A or B

where scheme is 44, 65 or 87

The app

```
app\build.bat
```

## Provision 

From the **repo root**:

```
python tools\provisioning.py --scheme 65 --app boards\LP_AM243\app\out\app.bin
```

## Flashing and Running the Program

Board in `UART` boot-mode for flashing (see "Boot-Mode Selection Table" in [AM243x LaunchPad™ Development Kit User's Guide](https://www.ti.com/lit/ug/spruj12f/spruj12f.pdf?ts=1791092648123&ref_url=https%253A%252F%252Fwww.ti.com%252Ftool%252FLP-AM243)).

Examples below showed on COM6. Replace with the COM number assigned on your machine.

Flash boot+enclave+pubkey+app

```
.\boot\scripts\flash.bat COM6 --with-sbl
```

Or (re-)flash just the boot;

```
.\boot\scripts\flash.bat COM6 --boot-only
```

Or (re-)flash just `signed_app.bin`;

```
.\boot\scripts\flash.bat COM6
```

 Or (re-)flash just the key (rotation, no rebuild needed on either project).

 ```
.\boot\scripts\flash.bat COM6 --pubkey
```

Switch to `QSPI Flash` boot-mode and power-cycle to run. 


