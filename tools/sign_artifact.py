#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from Crypto.Hash import SHA256
from Crypto.PublicKey import RSA
from Crypto.Signature import pkcs1_15


def sign_file(input_path: Path, private_key_path: Path, output_path: Path) -> None:
    data = input_path.read_bytes()
    key = RSA.import_key(private_key_path.read_bytes())
    digest = SHA256.new(data)
    signature = pkcs1_15.new(key).sign(digest)
    output_path.write_bytes(signature)


def main() -> int:
    parser = argparse.ArgumentParser(description="Sign a binary artifact with RSA/SHA-256.")
    parser.add_argument("input", type=Path)
    parser.add_argument("private_key", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    sign_file(args.input, args.private_key, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
