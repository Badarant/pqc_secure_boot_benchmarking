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
from dilithium_py.ml_dsa import ML_DSA_44, ML_DSA_65, ML_DSA_87

PAGE          = 0x1000
HEADER_PAGES  = 2                 # 8 KB header (2 pages) - uniform for all schemes
HEADER_BYTES  = HEADER_PAGES * PAGE
MAGIC         = 0x53344D50        # 'PM4S'
HDR_VERSION   = 1

# scheme table: algo_id -> (impl, sig_len, pk_len)
SCHEMES = {
    "44": (ML_DSA_44, 1, 2420, 1312),
    "65": (ML_DSA_65, 2, 3309, 1952),
    "87": (ML_DSA_87, 3, 4627, 2592),
}

HDR_FMT   = "<8I"                  # magic, version, image_size, algo_id, reserved[4]
HDR_BYTES = struct.calcsize(HDR_FMT)   # 32

def pad(data, size):
    if len(data) > size:
        sys.exit(f"error: {len(data)} B exceeds {size} B")
    return data + b"\xff" * (size - len(data))

def signable_header(image_size, algo_id):
    return struct.pack(HDR_FMT, MAGIC, HDR_VERSION, image_size, algo_id, 0,0,0,0)

def cmd_keygen(a):
    S, algo_id, _, pk_len = SCHEMES[a.scheme]
    os.makedirs(a.out, exist_ok=True)
    pk, sk = S.keygen()
    assert len(pk) == pk_len
    open(os.path.join(a.out, "private.key"), "wb").write(sk)
    open(os.path.join(a.out, "public.key"),  "wb").write(pk)
    open(os.path.join(a.out, "pubkey.bin"),  "wb").write(pad(pk, PAGE))
    print(f"ML-DSA-{a.scheme}: keys in {a.out}/  pk={len(pk)} -> flash pubkey.bin at 0x0801F000")

def cmd_sign(a):
    S, algo_id, sig_len, _ = SCHEMES[a.scheme]
    sk    = open(a.key, "rb").read()
    image = open(a.app, "rb").read()
    signable = signable_header(len(image), algo_id)
    digest   = hashlib.sha3_256(signable + image).digest()
    sig      = S.sign(sk, digest)
    assert len(sig) == sig_len, f"sig {len(sig)} != {sig_len}"
    header_page = pad(signable + sig, HEADER_BYTES)   # 8 KB
    signed      = header_page + image
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, "signed_app.bin"), "wb").write(signed)
    print(f"ML-DSA-{a.scheme}: image={len(image)} B, sig={len(sig)} B, algo_id={algo_id}")
    print(f"  signed_app.bin ({len(signed)} B) -> 0x08020000 (header@0x08020000, app@0x08022000)")

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(required=True)
    for cmd, fn in [("keygen", cmd_keygen), ("sign", cmd_sign)]:
        p = sub.add_parser(cmd)
        p.add_argument("--scheme", required=True, choices=["44","65","87"])
        p.add_argument("--out", default=cmd)
        if cmd == "sign":
            p.add_argument("--key", required=True)
            p.add_argument("--app", required=True)
        p.set_defaults(f=fn)
    a = ap.parse_args(); a.f(a)

if __name__ == "__main__":
    main()