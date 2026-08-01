# PQC Secure-Boot Benchmarking

This project intends to benchmark boot implementations for ARM, secured with different NIST-released PQC signing schemes and their variants.

## Description

Currently focusing on ARM Cortex cores: 
- M4 - using NUCLEO-L4R5ZI development board 

PQC schemes implementations are reused from third party implementation in:
https://github.com/mupq/pqm4
https://github.com/mupq/mupq/tree/ddcccedb2db9d0250856bd58ee5c46c61e506c5d
https://github.com/pqclean/pqclean/tree/c3e6861fbb0a0b2721d2599c0a68430061414f18

## Getting Started

### Dependencies

Project compiled under Ubuntu with arm-none-eabi-gcc

### Installing

For signing the app (firmware), the tools folder provides a Python signing script based on dilithium-py package. Install it accordingly:

```
python3 -m venv .venv && source .venv/bin/activate
pip install dilithium-py
```

Then generate your private and public keys once:
python3 secureboot-host-sign_app.py keygen --out keys

And use the private key to sign your app.bin after each app change:
```
python3 secureboot-host-sign_app.py sign --key keys/private.key --app app/app.bin --out signed
```

### Making the program

```
cd boot
make clean
make
cd ../app
make clean
make
```

## Help

## Authors

Liviu Silaghe liviu.silaghe@gmail.com

## Version History

* 0.1
    * Initial Release using PQClean implementation of ML-DSA-65 scheme for signing and verifying a simple app of ~260bytes

## License

This project is licensed according LICENSE file in the root folde
and according to the reused third party sw licenses present in the third_party_sw
