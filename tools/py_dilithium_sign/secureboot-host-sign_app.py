# Copyright 2026 Liviu Silaghe liviu.silaghe@gmail.com
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

#!/usr/bin/env python3
"""
Host-side signing tool for the PQC secure-boot PoC (ML-DSA-65, FIPS 204).

Flash layout produced:
  0x0801F000  pubkey.bin       public key (padded to a 4 KB page)
  0x08020000  header (4 KB)  ] combined into signed_app.bin, flashed as one
  0x08021000  application    ]

Signed message = SHA3-256( signable_header || app_image ), signed with pure ML-DSA.
The signable header covers magic/version/image_size/algo_id, so those fields are
tamper-protected. The signature itself sits AFTER the signable fields, so it is
naturally excluded from what it signs.

Dependencies: pip install dilithium-py
NOTE: dilithium-py is not side-channel safe. Fine for offline host signing;
never use it for on-device operations or production keys as-is.
"""
import argparse, hashlib, struct, os, sys
from dilithium_py.ml_dsa import ML_DSA_65

PAGE          = 0x2000
MAGIC         = 0x53344D50          # 'PM4S'
HDR_VERSION   = 1
ALGO_ML_DSA65 = 1
SIG_LEN       = 3309                # ML-DSA-65 signature length
PK_LEN        = 1952                # ML-DSA-65 public key length

# signable header = 8 x uint32 (little-endian) = 32 bytes
HDR_FMT   = "<8I"
HDR_BYTES = struct.calcsize(HDR_FMT)   # 32

def pad(data: bytes, size: int) -> bytes:
    if len(data) > size:
        sys.exit(f"error: {len(data)} bytes exceeds page size {size}")
    return data + b"\xff" * (size - len(data))   # 0xFF = erased flash

def build_signable_header(image_size: int) -> bytes:
    return struct.pack(HDR_FMT, MAGIC, HDR_VERSION, image_size,
                       ALGO_ML_DSA65, 0, 0, 0, 0)

def cmd_keygen(args):
    os.makedirs(args.out, exist_ok=True)
    pk, sk = ML_DSA_65.keygen()
    assert len(pk) == PK_LEN
    open(os.path.join(args.out, "private.key"), "wb").write(sk)
    open(os.path.join(args.out, "public.key"),  "wb").write(pk)
    # public key as a page-aligned flash image for 0x0801F000
    open(os.path.join(args.out, "pubkey.bin"), "wb").write(pad(pk, PAGE))
    print(f"keys written to {args.out}/  (private.key, public.key, pubkey.bin)")
    print(f"  pk={len(pk)}  sk={len(sk)}  -> flash pubkey.bin at 0x0801F000")

def cmd_sign(args):
    sk    = open(args.key, "rb").read()
    image = open(args.app, "rb").read()
    image_size = len(image)

    signable = build_signable_header(image_size)
    digest   = hashlib.sha3_256(signable + image).digest()
    sig      = ML_DSA_65.sign(sk, digest)
    assert len(sig) == SIG_LEN, f"unexpected sig len {len(sig)}"

    # full header page: signable fields + signature, padded to a 4 KB page
    header_page = pad(signable + sig, PAGE)
    # combined image to flash at 0x08020000 in one shot: header page + app
    signed = header_page + image

    os.makedirs(args.out, exist_ok=True)
    open(os.path.join(args.out, "header.bin"),     "wb").write(header_page)
    open(os.path.join(args.out, "signed_app.bin"), "wb").write(signed)
    print(f"signed: image={image_size} B, digest={digest.hex()[:16]}...")
    print(f"  header.bin     ({len(header_page)} B) -> 0x08020000")
    print(f"  signed_app.bin ({len(signed)} B)      -> 0x08020000 (header+app in one flash)")

def main():
    ap = argparse.ArgumentParser(description="PQC secure-boot signing tool (ML-DSA-65)")
    sub = ap.add_subparsers(required=True)
    k = sub.add_parser("keygen"); k.add_argument("--out", default="keys"); k.set_defaults(f=cmd_keygen)
    s = sub.add_parser("sign")
    s.add_argument("--key", required=True); s.add_argument("--app", required=True)
    s.add_argument("--out", default="signed"); s.set_defaults(f=cmd_sign)
    a = ap.parse_args(); a.f(a)

if __name__ == "__main__":
    main()
