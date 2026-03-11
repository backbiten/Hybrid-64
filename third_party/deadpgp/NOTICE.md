# NOTICE — vendored third-party code: deadpgp

| Field             | Value                                                       |
|-------------------|-------------------------------------------------------------|
| **Original repo** | https://github.com/backbiten/deadpgp                        |
| **Vendored SHA**  | `3e2ef0035f386b518a84e8c1167330a53d86f6d6`                  |
| **License**       | Eclipse Public License v2.0 (EPL-2.0)                       |
| **Modifications** | None — imported as-is from the upstream repository          |

> **Known issue (upstream):** In `tools/openpgp_import/import.py`, the `--homedir` flag is
> inserted at index 0 (before the `gpg` binary name), which would cause the subprocess call
> to fail when `--homedir` is used. This is a pre-existing bug in the upstream code and has
> been left unmodified to preserve the as-is import.

## About deadpgp

deadpgp is an early-stage project that forks old PGP protocols from the 1980s–1990s and reframes them for
modern, safety-focused workflows. Its current tooling includes a small Python CLI that wraps `gpg --decrypt`
for OpenPGP file import, with a roadmap toward orchestration and policy layers over GnuPG/OpenPGP.

## License note

All files under `third_party/deadpgp/` are covered by the **Eclipse Public License v2.0** (EPL-2.0),
a copy of which is provided in `third_party/deadpgp/LICENSE`.

The rest of the Hybrid-64 repository is covered by the **MIT License** (see top-level `LICENSE`).

These licenses are compatible for this vendoring arrangement: the EPL-2.0 code is kept as a distinct
subtree and is not modified or statically linked into the MIT-licensed core.
