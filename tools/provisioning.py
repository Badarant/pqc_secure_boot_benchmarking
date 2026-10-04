# SPDX-License-Identifier: Apache-2.0
#
# provisioning.py - convenience wrapper around the ML-DSA signer next to this
# file (py_dilithium_sign/secureboot-host-sign_app.py). Run from the repo root:
#
#   python tools/provisioning.py --scheme 65
#   python tools/provisioning.py --scheme 65 --app my_app.bin --keep-keys
#   python tools/provisioning.py --scheme 65 --app my_app.bin --pad-to 1048576
#
# Produces, under the repo root (both gitignored):
#   keys/private.key, keys/public.key, keys/pubkey.bin   (page-padded, standalone)
#   out/signed_app.bin                                    (the signed application)
#
# pubkey.bin carries no header and is never compiled into a board's `boot`:
# `boot` reads it at a fixed flash address at runtime, so rotating the key is
# "flash a new pubkey.bin", never a rebuild. Equivalent to running the signer
# directly (see boards/LP_AM243/boot/README.md for the exact flash offsets):
#
#   python tools/py_dilithium_sign/secureboot-host-sign_app.py keygen --scheme 65 --hash SHA_256 --out keys
#   python tools/py_dilithium_sign/secureboot-host-sign_app.py sign   --scheme 65 --hash SHA_256 --key keys/private.key --app <app> --out out
#
# Requires: pip install dilithium-py

import argparse, os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SIGNER = os.path.join(HERE, "py_dilithium_sign", "secureboot-host-sign_app.py")

# Padding pattern for --pad-to: repeating, human-recognisable in a hex dump,
# never all-zero (so it cannot be mistaken for unwritten flash/.bss).
PAD_PATTERN = b"PQSB-PAD"


def run(*cmd):
    print("+", " ".join(str(c) for c in cmd))
    subprocess.run([sys.executable, *map(str, cmd)], check=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--scheme", default="65", choices=["44", "65", "87"])
    ap.add_argument("--app", help="payload to sign (default: a generated test blob)")
    ap.add_argument("--keep-keys", action="store_true",
                    help="do not regenerate keys if keys/private.key already exists")
    ap.add_argument("--pad-to", type=int, metavar="BYTES",
                    help="pad --app with a repeating filler pattern up to this many "
                         "bytes (never truncates; error if --app is already bigger). "
                         "For sizing the image_size benchmark.")
    ap.add_argument("--keys-dir", default=os.path.join(REPO, "keys"),
                    help="default: <repo root>/keys")
    ap.add_argument("--out-dir", default=os.path.join(REPO, "out"),
                    help="default: <repo root>/out")
    a = ap.parse_args()

    keys = a.keys_dir
    out = a.out_dir
    os.makedirs(keys, exist_ok=True)
    os.makedirs(out, exist_ok=True)

    priv = os.path.join(keys, "private.key")
    if not (a.keep_keys and os.path.exists(priv)):
        run(SIGNER, "keygen", "--scheme", a.scheme, "--hash", "SHA_256", "--out", keys)

    app = a.app
    if not app:
        app = os.path.join(out, "test_app.bin")
        # Deterministic, recognisable filler. Opaque to boot.
        payload = bytearray()
        for i in range(4096):
            payload += bytes([(i * 7 + 0x11) & 0xFF])
        payload[0:16] = b"PQSB-TEST-APP\x00\x00\x00"
        open(app, "wb").write(payload)
        print(f"wrote {app} ({len(payload)} B test payload)")

    if a.pad_to is not None:
        src = open(app, "rb").read()
        if len(src) > a.pad_to:
            sys.exit(f"error: --app is {len(src)} B, bigger than --pad-to {a.pad_to} B "
                     f"(padding only grows, never truncates)")
        pad_len = a.pad_to - len(src)
        pad = (PAD_PATTERN * (pad_len // len(PAD_PATTERN) + 1))[:pad_len]
        padded = os.path.join(out, "app_padded.bin")
        open(padded, "wb").write(src + pad)
        print(f"wrote {padded} ({len(src)} B app + {pad_len} B padding = {a.pad_to} B)")
        app = padded

    run(SIGNER, "sign", "--scheme", a.scheme, "--hash", "SHA_256",
        "--key", priv, "--app", app, "--out", out)

    pubkey = os.path.join(keys, "pubkey.bin")
    signed = os.path.join(out, "signed_app.bin")
    print("\nprovision OK:")
    print(f"  {pubkey}   (standalone, no header - flash alongside boot, not compiled in)")
    print(f"  {signed} (flash separately; can be updated independently of boot+pubkey)")


if __name__ == "__main__":
    main()
