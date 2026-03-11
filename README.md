# Hybrid-64

> A cross-platform, portable drive abstraction built from Legacy 64 repos —
> targeting **x86-64**, **arm64**, and **IoT microcontrollers** (bare-metal/RTOS).

---

## What Is Hybrid-64?

Hybrid-64 is a lightweight, freestanding C library that provides a minimal
"drive" abstraction (open / close / read / write / flush) with an optional
cryptographic/authentication layer.

The design separates a **portable core** (`core/`) from thin
**platform adapters** (`platform/posix` and `platform/mcu`) so that the same
logic runs on a 64-bit desktop/server and on a resource-constrained
microcontroller without dynamic allocation or an OS.

Key properties:
- **No dynamic allocation** in the core by default — safe for MCUs with a few
  KB of RAM.
- **Deterministic behaviour** — no hidden allocations, no global mutable state
  outside explicitly documented structures.
- **Freestanding C** — depends only on `stdint.h`, `stddef.h`, and a handful
  of compiler built-ins; no libc required in the core.
- **Platform adapters** plug in POSIX file I/O on Linux/macOS and HAL
  primitives on MCUs.

---

## Target Platforms

| Platform | Notes |
|---|---|
| x86-64 Linux | Primary development + CI host |
| arm64 Linux | Raspberry Pi, embedded Linux boards |
| Bare-metal MCU | ESP32, STM32, RP2040, nRF52 — no OS, static buffers only |

---

## Project Status

**As of 2026-03-11 — Documentation phase.**

| Item | Status |
|---|---|
| `docs/SPEC.md` — architecture spec | ✅ Added |
| `README.md` — this file | ✅ Updated |
| `core/` — portable C core | ⬜ Planned (next PR) |
| `platform/posix` adapter | ⬜ Planned |
| `platform/mcu` adapter | ⬜ Planned |
| CI (cross-compile + static analysis) | ⬜ Planned |

---

## Repository Layout (planned)

```
Hybrid-64/
├── core/               # Portable, freestanding C (no libc dependencies)
│   ├── include/        # Public headers — drive.h, error.h, version.h
│   └── src/            # Core logic
├── platform/
│   ├── posix/          # POSIX adapter (Linux x86-64 / arm64)
│   └── mcu/            # MCU adapter (bare-metal / RTOS stubs)
├── docs/
│   └── SPEC.md         # Full specification (this PR)
├── LICENSE
├── .gitignore
└── README.md
```

---

## Documentation

- **[docs/SPEC.md](docs/SPEC.md)** — Full architecture specification, API
  surface, error model, threat model, and build/CI expectations.

---

## How to Contribute

1. Read [`docs/SPEC.md`](docs/SPEC.md) before writing any code.
2. Keep all core changes **portable and freestanding** — no libc, no OS calls.
3. Every public API change must be reflected in `docs/SPEC.md` first (spec
   drives implementation, not the other way around).
4. Static analysis (`cppcheck`, `clang-tidy`) must pass before merging.
5. Cross-compile checks for at least `x86_64-linux-gnu` and
   `aarch64-linux-gnu` are required for `core/` changes.
6. Open a PR against `main`; include a short description of *why* the change
   is needed and reference the relevant section of `SPEC.md`.

---

## Signing

Release integrity is maintained via **Git tag signing**:

- Maintainers sign release tags with a GPG key:
  ```
  git tag -s v0.1.0 -m "Release v0.1.0"
  git push origin v0.1.0
  ```
- Contributors are encouraged to sign their commits (`git commit -S`).
- Each release will carry a `SHA-256` checksum file alongside the source
  archive (`hybrid64-<version>.tar.gz.sha256`).
- The public key used to sign releases will be published in `docs/KEYS.asc`
  once the first release is cut.

> **Note:** No external signing service is required. Standard GPG (GnuPG) or
> an SSH signing key configured in `~/.gitconfig` is sufficient.

### Sign-off block (current, pre-release)

The specification in `docs/SPEC.md` has been reviewed and accepted by the
project maintainer:

| Field | Value |
|---|---|
| Spec version | 0.1-draft |
| Review date | 2026-03-11 |
| Maintainer | backbiten |
| SHA-256 of `docs/SPEC.md` | *(computed at first tag)* |

---

## License

MIT — see [`LICENSE`](LICENSE).

