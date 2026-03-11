#!/usr/bin/env python3
"""
tools/deadpgp_sign_docs.py
--------------------------
Hybrid-64 wrapper for deadpgp tooling.

This script provides a thin interface to the vendored deadpgp utilities
(under third_party/deadpgp/) for signing and verifying Hybrid-64 release
artifacts and documentation files.

Usage:
    python tools/deadpgp_sign_docs.py decrypt <infile> <outfile> [--homedir DIR]
    python tools/deadpgp_sign_docs.py verify  <file> <sigfile>  [--homedir DIR]
    python tools/deadpgp_sign_docs.py sign    <file>            [--homedir DIR]

Notes:
    - This script is OPTIONAL and does NOT affect the freestanding C core.
    - No network calls are made.
    - Never commit private keys; only public keys (e.g. docs/KEYS.asc) may be
      committed.
    - Requires GnuPG (gpg) to be installed on the host system.
    - All deadpgp source lives under third_party/deadpgp/ (EPL-2.0 license).
"""

import argparse
import subprocess
import sys
import os

# Resolve the vendored deadpgp path relative to this script's location.
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEADPGP_IMPORT = os.path.join(
    REPO_ROOT, "third_party", "deadpgp", "tools", "openpgp_import", "import.py"
)


def _gpg_base(homedir=None):
    """Return the base gpg command list, optionally with --homedir."""
    cmd = ["gpg"]
    if homedir:
        cmd += ["--homedir", homedir]
    return cmd


def cmd_decrypt(args):
    """Decrypt an OpenPGP-encrypted file using the vendored deadpgp import.py."""
    python = sys.executable
    command = [python, DEADPGP_IMPORT, args.infile, args.outfile]
    if args.homedir:
        command += ["--homedir", args.homedir]
    subprocess.run(command, check=True)


def cmd_verify(args):
    """Verify a detached GPG signature for a file.

    Placeholder: calls gpg --verify directly.
    Future: route through deadpgp policy/audit layer when available upstream.
    """
    command = _gpg_base(args.homedir) + ["--verify", args.sigfile, args.file]
    result = subprocess.run(command)
    if result.returncode != 0:
        print(f"[deadpgp_sign_docs] Signature verification FAILED for {args.file}",
              file=sys.stderr)
        sys.exit(result.returncode)
    print(f"[deadpgp_sign_docs] Signature OK: {args.file}")


def cmd_sign(args):
    """Create a detached GPG signature for a file.

    Placeholder: calls gpg --detach-sign directly.
    Future: route through deadpgp orchestration layer when available upstream.
    """
    command = _gpg_base(args.homedir) + ["--detach-sign", "--armor", args.file]
    result = subprocess.run(command)
    if result.returncode != 0:
        print(f"[deadpgp_sign_docs] Signing FAILED for {args.file}", file=sys.stderr)
        sys.exit(result.returncode)
    print(f"[deadpgp_sign_docs] Signed: {args.file} -> {args.file}.asc")


def main():
    parser = argparse.ArgumentParser(
        description="Hybrid-64 wrapper for deadpgp signing/verification utilities."
    )
    parser.add_argument(
        "--homedir",
        default=None,
        help="Alternate GnuPG home directory (passed through to gpg).",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    # decrypt subcommand
    p_decrypt = subparsers.add_parser(
        "decrypt", help="Decrypt an OpenPGP-encrypted file via deadpgp import.py."
    )
    p_decrypt.add_argument("infile", help="Input .gpg file to decrypt.")
    p_decrypt.add_argument("outfile", help="Output plaintext file path.")
    p_decrypt.set_defaults(func=cmd_decrypt)

    # verify subcommand
    p_verify = subparsers.add_parser(
        "verify", help="Verify a detached GPG signature for a file."
    )
    p_verify.add_argument("file", help="File whose signature should be verified.")
    p_verify.add_argument("sigfile", help="Detached signature file (.asc or .sig).")
    p_verify.set_defaults(func=cmd_verify)

    # sign subcommand
    p_sign = subparsers.add_parser(
        "sign", help="Create a detached GPG signature for a file."
    )
    p_sign.add_argument("file", help="File to sign.")
    p_sign.set_defaults(func=cmd_sign)

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
