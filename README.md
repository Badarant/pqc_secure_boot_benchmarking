# PQC Secure-Boot Benchmarking

This project intends to benchmark boot implementations for ARM, secured with different NIST-released PQC signing schemes and their variants.

## Description

Currently focusing on ARM Cortex cores: 
- M4F - using NUCLEO-L4R5ZI development board 
- R5F - using AM243 LP development board 

PQC schemes implementations are reused from third party implementation in:

https://github.com/mupq/pqm4

https://github.com/mupq/mupq/tree/ddcccedb2db9d0250856bd58ee5c46c61e506c5d

https://github.com/pqclean/pqclean/tree/c3e6861fbb0a0b2721d2599c0a68430061414f18

## Getting Started

### Cloning the repo
```
git clone https://github.com/Badarant/pqc_secure_boot_benchmarking.git
cd pqc_secure_boot_benchmarking/
git submodule update --init --recursive
```

## Installing, Making and Flashing the program

[Nucleo L4R5ZI board](boards/nucleo_L4R5ZI/Nucleo_L4R5ZI_README.md)

[AM243LP board](boards/LP_AM243/LP_AM243_README.md)


## Authors

Liviu Silaghe liviu.silaghe@gmail.com


## License

This project is licensed according LICENSE file in the root folder and according to the reused third party sw licenses present in the third_party_sw folder. The third_party_sw\pqm4 submodule includes benchmark tooling with additional dependencies (e.g. tqdm, MPL-2.0) that are not used, compiled, or distributed by this project.
