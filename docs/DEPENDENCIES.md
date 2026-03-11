# Dependencies

This document describes third-party dependencies vendored into Hybrid-64.

---

## deadpgp

| Field             | Value                                             |
|-------------------|---------------------------------------------------|
| **Source**        | https://github.com/backbiten/deadpgp              |
| **Vendored path** | `third_party/deadpgp/`                            |
| **Vendored SHA**  | `3e2ef0035f386b518a84e8c1167330a53d86f6d6`        |
| **License**       | Eclipse Public License v2.0 (EPL-2.0)             |
| **Modifications** | None — imported as-is                             |

### What it is

deadpgp is an early-stage Python tooling library that wraps GnuPG/OpenPGP for:

- Decrypting OpenPGP-encrypted files via a CLI shim around `gpg --decrypt`
- (Roadmap) Sequenced "reveal" workflows with multi-party approval policies
- (Roadmap) Auditable JSONL decision logs

### How Hybrid-64 uses deadpgp

Hybrid-64 uses deadpgp as **local tooling only** — it is not part of the freestanding C core and does
not affect MCU or bare-metal builds. Specifically:

- `tools/deadpgp_sign_docs.py` — thin wrapper that calls deadpgp's `import.py` to decrypt/verify
  release artifacts and documentation files before publishing.
- Future: signing `docs/SPEC.md` and `README.md` via `gpg --detach-sign`, verified with deadpgp
  workflow patterns.

### Licensing notes

- The Hybrid-64 C core (`core/`, `platform/`) is **MIT** licensed (see top-level `LICENSE`).
- The vendored deadpgp subtree (`third_party/deadpgp/`) is **EPL-2.0** licensed (see
  `third_party/deadpgp/LICENSE`).
- The `tools/` wrapper scripts in this repo are **MIT** licensed and only call into deadpgp at
  runtime via subprocess / Python import; they are not statically linked into the deadpgp code.
- Attribution is preserved in `third_party/deadpgp/NOTICE.md`.

No private keys are committed. The tooling is entirely optional and offline.
