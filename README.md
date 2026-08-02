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
```
python secureboot-host-sign_app.py keygen --scheme 44 --out keys_44
python secureboot-host-sign_app.py keygen --scheme 65 --out keys_65
python secureboot-host-sign_app.py keygen --scheme 87 --out keys_87
```

### Making the program

```
cd boot
make clean-build 
make <required scheme implementation>
cd ../app
make clean
make
```
| Required Scheme Implementation| make option|

|-----------------:|----------------:|

|   Clean ML-DSA-44|    bench-clean-44|
|   M4F ML-DSA-44|    bench-m4f-44|
|   Clean ML-DSA-65|    bench-clean-65|
|   M4F ML-DSA-44|    bench-m4f-65|
|   Clean ML-DSA-87|    bench-clean-87|
|   M4F ML-DSA-87|    bench-m4f-87|

### Signing the program

After successful And use the private key in tools/py_dilithium_sign/ to sign your app.bin after each app change:
```
python secureboot-host-sign_app.py sign --scheme 44 --key keys_44/private.key --app ../../app/app.bin --out signed_44
python secureboot-host-sign_app.py sign --scheme 65 --key keys_65/private.key --app ../../app/app.bin --out signed_65
python secureboot-host-sign_app.py sign --scheme 87 --key keys_87/private.key --app ../../app/app.bin --out signed_87
```

## Help

## Authors

Liviu Silaghe liviu.silaghe@gmail.com

## Version History
* 0.2
    * Include both Clean and M4F implementations for all 3 schemes ML-DSA-44, ML-DSA-65 and ML-DSA-87
* 0.1
    * Initial Release using PQClean implementation of ML-DSA-65 scheme for signing and verifying a simple app of ~260bytes

## License

This project is licensed according LICENSE file in the root folder and according to the reused third party sw licenses present in the third_party_sw folder
