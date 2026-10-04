## Making the program


Project compiled under Ubuntu with arm-none-eabi-gcc

```
cd boards/nucleo_L4R5ZI/boot
make clean-build 
make (required scheme implementation) HASH=(required hash) FREQ=(required frequency)
cd ../app
make clean
make
```

(required scheme implementation) in [bench-clean-44, bench-m4f-44, bench-clean-65, bench-m4f-65, bench-clean-87, bench-m4f-87]

(required frequency) in [4,20,40,80,120]

(required hash) in [SHA_256, SHA3_256]

## Signing the app

For signing the app (firmware), the tools folder provides a Python signing script based on dilithium-py package. (dilithium-py by Giacomo Pope, dual-licensed MIT/Apache-2.0)

Install it accordingly:
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


And use the private key in tools/py_dilithium_sign/ to sign your app.bin after each app change, for the respective scheme:
```
python secureboot-host-sign_app.py sign --scheme 44 --hash (required hash) --key keys_44/private.key --app ../../app/app.bin --out signed_44
python secureboot-host-sign_app.py sign --scheme 65 --hash (required hash) --key keys_65/private.key --app ../../app/app.bin --out signed_65
python secureboot-host-sign_app.py sign --scheme 87 --hash (required hash) --key keys_87/private.key --app ../../app/app.bin --out signed_87
```

(required hash) in [SHA_256, SHA3_256]


## Flashing the binaries


Install OpenOCD from https://openocd.org/.

Then flash the pubkey.bin file for your respective scheme:
e.g:
```
openocd -f board/st_nucleo_l4.cfg -c "program keys_87/pubkey.bin 0x0801F000 verify reset exit"
```

Then flash the signed app:
```
openocd -f board/st_nucleo_l4.cfg -c "program signed_87/signed_app.bin 0x08020000 verify reset exit"
```

Then flash the binary:
```
openocd -f board/st_nucleo_l4.cfg -c "program ../../boards/nucleo_L4R5ZI/boot/boot-m4f-ml-dsa-87.bin 0x08000000 verify reset exit"
```
## Reading the results


To monitor the UART interface of th Nucleo board, you can use tty under Ubuntu:

```
stty -F /dev/ttyACM0 9600 raw
cat /dev/ttyACM0 > myFileExample.txt
```


The boot will print on UART the ML-DSA scheme used, the stack usage, the frequency at which it runs, the SHA3 and Verify Cycles Count, as well as the verification verdict:

```
M4F-ML-DSA-87 frequency [MHz]: 4
image_size: 264
sha3 cycles:   33265
verify cycles: 4309934
total cycles:  4343199
stack usage:  12200bytes
VALID -> boot
```