# Hybrid-64 Drive — Specification

**Version:** 0.1-draft  
**Date:** 2026-03-11  
**Status:** Draft — subject to change before v0.1.0 tag  
**Maintainer:** backbiten

---

## Table of Contents

1. [Overview](#1-overview)
2. [Goals and Non-Goals](#2-goals-and-non-goals)
3. [Architecture](#3-architecture)
4. [API Surface](#4-api-surface)
5. [Error Model](#5-error-model)
6. [Logging and Audit Model](#6-logging-and-audit-model)
7. [Versioning](#7-versioning)
8. [Threat Model and Safety Requirements](#8-threat-model-and-safety-requirements)
9. [Build and CI Expectations](#9-build-and-ci-expectations)
10. [Legacy 64 Migration](#10-legacy-64-migration)

---

## 1. Overview

Hybrid-64 is a cross-platform, portable "drive" abstraction library written in
freestanding C. It provides a minimal block-oriented I/O interface — open,
close, read, write, flush — with an optional cryptographic/authentication layer
that can be compiled in or out.

The library is designed to run identically on:
- 64-bit Linux hosts (x86-64 and arm64)
- Bare-metal or RTOS microcontrollers (e.g., ESP32, STM32, RP2040, nRF52)
  with no operating system, very limited RAM and flash, no filesystem, and no
  dynamic allocation.

The architectural split — a portable `core/` plus thin `platform/` adapters —
allows the same business logic and safety guarantees to be shared across all
targets without conditional compilation spread throughout the library.

---

## 2. Goals and Non-Goals

### 2.1 Goals

- **Portability** — `core/` must compile on any C11-capable compiler targeting
  freestanding (bare-metal) environments. No libc, no POSIX, no OS headers in
  `core/`.
- **Correctness over performance** — safe defaults, explicit bounds checks, no
  undefined behaviour in reachable code paths.
- **Minimal footprint** — suitable for MCUs with as little as 8 KB of RAM and
  64 KB of flash.
- **Deterministic behaviour** — no dynamic allocation in the core by default;
  all buffers are caller-supplied or statically allocated.
- **Auditability** — clear error codes, optional structured audit logging, and
  a threat model that is part of the specification.

### 2.2 Non-Goals

- **Full POSIX filesystem semantics** — Hybrid-64 is a drive abstraction, not
  a filesystem.
- **Multi-threading safety in core** — thread safety is the responsibility of
  the platform adapter or the caller.
- **Dynamic allocation in core** — `malloc`/`free` are never called in `core/`.
  Platform adapters may use dynamic allocation if the platform supports it.
- **OS-level features on MCUs** — no assumption of an OS, scheduler, or
  virtual memory on MCU targets.
- **Network I/O** — out of scope for this library.
- **GUI or user interface** — out of scope.

### 2.3 MCU-Specific Constraints

| Constraint | Requirement |
|---|---|
| OS | None required; RTOS optional |
| Dynamic allocation | Prohibited in `core/` |
| RAM budget | Core data structures ≤ 512 bytes per drive handle by default |
| Flash budget | Core object code target ≤ 8 KB on Cortex-M0+ |
| Filesystem | None assumed; platform adapter provides raw sector I/O |
| Interrupts | Core functions must be safe to call from a single execution context |
| Floating point | Not used in `core/`; soft-float or no-FPU targets must compile cleanly |

---

## 3. Architecture

### 3.1 Directory Layout

```
Hybrid-64/
├── core/
│   ├── include/
│   │   ├── hybrid64/drive.h       # Primary drive abstraction header
│   │   ├── hybrid64/error.h       # Error codes
│   │   ├── hybrid64/crypto.h      # Optional crypto/auth layer header
│   │   └── hybrid64/version.h     # Version macros
│   └── src/
│       ├── drive.c                # Core drive logic
│       └── crc.c                  # CRC/checksum helpers (no libc)
├── platform/
│   ├── posix/
│   │   ├── include/               # POSIX-specific overrides
│   │   └── src/
│   │       └── posix_drive.c      # POSIX file I/O adapter
│   └── mcu/
│       ├── include/               # MCU HAL interface definitions
│       └── src/
│           └── mcu_drive.c        # MCU HAL adapter (stub to be filled per board)
├── docs/
│   └── SPEC.md                    # This file
├── LICENSE
├── .gitignore
└── README.md
```

### 3.2 Layer Responsibilities

#### `core/`

- All code in `core/` must compile with `-ffreestanding -nostdlib` (or the
  MSVC equivalent).
- Permitted headers: `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`,
  `<limits.h>`, `<stdarg.h>`.
- No calls to any function from `<stdio.h>`, `<stdlib.h>`, `<string.h>`, or
  any POSIX header.
- Implements the drive state machine, bounds checking, error propagation, and
  optional crypto integration points.

#### `platform/posix/`

- Targets Linux (x86-64 and arm64) and macOS.
- Implements the `hybrid64_platform_ops_t` vtable using POSIX `open(2)`,
  `read(2)`, `write(2)`, `fsync(2)`, `close(2)`.
- May use `<stdio.h>` and `<stdlib.h>` for logging and utilities.
- Must cross-compile cleanly for both `x86_64-linux-gnu` and
  `aarch64-linux-gnu` toolchains.

#### `platform/mcu/`

- Targets bare-metal and RTOS environments.
- Implements the `hybrid64_platform_ops_t` vtable using board-specific HAL
  calls (e.g., `HAL_FLASH_Program`, SPI flash routines).
- No dynamic allocation; all buffers must be supplied by the caller or be
  statically declared.
- Provides stub implementations that return `H64_ERR_NOT_IMPLEMENTED` so that
  the rest of the core links correctly on a new board before HAL integration.

### 3.3 Platform Ops Vtable

The abstraction boundary between `core/` and `platform/` is the
`hybrid64_platform_ops_t` struct (defined in `core/include/hybrid64/drive.h`).
Platform adapters fill in this struct and pass it to `hybrid64_open()`.

```c
typedef struct hybrid64_platform_ops {
    /* Open the underlying storage. Returns H64_OK or error code. */
    int (*open)(void *ctx, const char *path, int flags);

    /* Close the underlying storage. */
    int (*close)(void *ctx);

    /* Read exactly 'len' bytes at byte offset 'offset' into 'buf'.
     * Returns number of bytes read, or negative error code. */
    int (*read)(void *ctx, uint64_t offset, void *buf, size_t len);

    /* Write exactly 'len' bytes at byte offset 'offset' from 'buf'.
     * Returns number of bytes written, or negative error code. */
    int (*write)(void *ctx, uint64_t offset, const void *buf, size_t len);

    /* Flush/sync pending writes to persistent storage. */
    int (*flush)(void *ctx);
} hybrid64_platform_ops_t;
```

All fields are required. A platform adapter that does not support a given
operation must supply a stub that returns `H64_ERR_NOT_IMPLEMENTED`.

---

## 4. API Surface

### 4.1 Drive Lifecycle

```c
/* drive.h */

/* Opaque handle; size defined in drive.h so callers can allocate it
 * statically: hybrid64_drive_t drv; */
typedef struct hybrid64_drive hybrid64_drive_t;

/* Initialise a drive handle. Must be called before any other function.
 * 'ops' must remain valid for the lifetime of the handle. */
int hybrid64_init(hybrid64_drive_t *drv,
                  const hybrid64_platform_ops_t *ops,
                  void *platform_ctx);

/* Open the drive at 'path' (platform-specific meaning).
 * 'flags': bitmask of H64_FLAG_* constants. */
int hybrid64_open(hybrid64_drive_t *drv, const char *path, uint32_t flags);

/* Close the drive and release all internal state.
 * The handle may be re-used after another call to hybrid64_init(). */
int hybrid64_close(hybrid64_drive_t *drv);

/* Read 'len' bytes from 'offset' into 'buf'. */
int hybrid64_read(hybrid64_drive_t *drv,
                  uint64_t offset, void *buf, size_t len);

/* Write 'len' bytes from 'buf' to 'offset'. */
int hybrid64_write(hybrid64_drive_t *drv,
                   uint64_t offset, const void *buf, size_t len);

/* Flush pending writes to persistent storage. */
int hybrid64_flush(hybrid64_drive_t *drv);
```

### 4.2 Open Flags

```c
#define H64_FLAG_RDONLY   (1u << 0)   /* Open read-only */
#define H64_FLAG_RDWR     (1u << 1)   /* Open read-write */
#define H64_FLAG_CREAT    (1u << 2)   /* Create if not exists (POSIX only) */
#define H64_FLAG_TRUNC    (1u << 3)   /* Truncate on open (POSIX only) */
#define H64_FLAG_SYNC     (1u << 4)   /* Synchronous writes (flush after each write) */
#define H64_FLAG_CRYPTO   (1u << 5)   /* Enable optional crypto layer */
```

Flags marked "(POSIX only)" are silently ignored by the MCU adapter.

### 4.3 Optional Crypto/Auth Layer

When `H64_FLAG_CRYPTO` is set, the core passes data through the crypto hooks
defined in `hybrid64/crypto.h` before writing and after reading. The crypto
interface is intentionally minimal and pluggable:

```c
/* crypto.h */

typedef struct hybrid64_crypto_ops {
    /* Encrypt 'len' bytes from 'plaintext' into 'ciphertext'.
     * Both buffers are caller-provided and non-overlapping.
     * Returns H64_OK or error code. */
    int (*encrypt)(void *ctx,
                   const void *plaintext, size_t len,
                   void *ciphertext, size_t ciphertext_buf_len);

    /* Decrypt 'len' bytes from 'ciphertext' into 'plaintext'. */
    int (*decrypt)(void *ctx,
                   const void *ciphertext, size_t len,
                   void *plaintext, size_t plaintext_buf_len);

    /* Optional: verify an integrity tag over 'data'. Returns H64_OK if valid. */
    int (*verify)(void *ctx, const void *data, size_t len,
                  const void *tag, size_t tag_len);
} hybrid64_crypto_ops_t;

/* Attach crypto ops to a drive handle.
 * Must be called after hybrid64_init() and before hybrid64_open(). */
int hybrid64_set_crypto(hybrid64_drive_t *drv,
                        const hybrid64_crypto_ops_t *crypto_ops,
                        void *crypto_ctx);
```

The default build (`H64_FLAG_CRYPTO` not set) compiles out all crypto code
paths, allowing the crypto module to be excluded from the MCU firmware image
entirely via a compile-time guard (`#ifdef HYBRID64_CRYPTO_ENABLED`).

---

## 5. Error Model

### 5.1 Return Codes

All public API functions return `int`. A return value of `0` (`H64_OK`)
indicates success. Negative values indicate errors. Positive values from
read/write indicate bytes transferred.

```c
/* error.h */

#define H64_OK                  (0)
#define H64_ERR_INVALID_ARG     (-1)   /* NULL pointer or invalid flag */
#define H64_ERR_NOT_OPEN        (-2)   /* Operation called before open */
#define H64_ERR_ALREADY_OPEN    (-3)   /* Drive is already open */
#define H64_ERR_IO              (-4)   /* Underlying I/O error */
#define H64_ERR_BOUNDS          (-5)   /* Offset + len exceeds drive capacity */
#define H64_ERR_CRYPTO          (-6)   /* Crypto operation failed */
#define H64_ERR_NOT_IMPLEMENTED (-7)   /* Platform adapter stub */
#define H64_ERR_BUFFER_TOO_SMALL (-8)  /* Caller buffer insufficient */
```

### 5.2 Rules

1. **No error is silently swallowed.** Every internal error must propagate to
   the caller via the return value.
2. **Invalid pointer arguments** (`NULL` where a non-`NULL` pointer is
   required) must return `H64_ERR_INVALID_ARG` immediately without
   dereferencing.
3. **Partial reads/writes are not permitted.** A read or write either
   succeeds completely (returns exact `len`) or returns a negative error code.
   The state of the destination buffer on error is undefined.
4. **After any error**, the drive handle remains in a well-defined state: it
   may be closed and re-opened, but data integrity after a write error is not
   guaranteed.

---

## 6. Logging and Audit Model

### 6.1 Principles

- The core does **not** call `printf`, `syslog`, or any I/O function directly.
- Logging is injected via an optional callback registered at init time.
- If no callback is registered, log events are silently discarded.
- On MCU targets, the callback is typically a UART write; on POSIX, it may
  write to `stderr` or a structured log file.

### 6.2 Log Hook

```c
typedef enum hybrid64_log_level {
    H64_LOG_DEBUG = 0,
    H64_LOG_INFO  = 1,
    H64_LOG_WARN  = 2,
    H64_LOG_ERROR = 3,
    H64_LOG_AUDIT = 4,   /* Security-relevant events (auth, crypto, open/close) */
} hybrid64_log_level_t;

/* Log callback type. 'msg' is a NUL-terminated string.
 * The callback must not call back into hybrid64 functions. */
typedef void (*hybrid64_log_fn_t)(hybrid64_log_level_t level,
                                  const char *msg,
                                  void *user_data);

/* Register a log callback. Pass NULL to disable logging. */
void hybrid64_set_log(hybrid64_drive_t *drv,
                      hybrid64_log_fn_t fn,
                      void *user_data);
```

### 6.3 Audit Events

The following events must always be logged at `H64_LOG_AUDIT` level when a log
callback is registered:

| Event | Logged fields |
|---|---|
| `hybrid64_open` success | path (truncated to 64 chars), flags, timestamp (if platform provides one) |
| `hybrid64_open` failure | path, flags, error code |
| `hybrid64_close` | handle identity |
| Crypto verify failure | offset, error code |
| `H64_ERR_BOUNDS` triggered | offset, requested len, drive capacity |

---

## 7. Versioning

### 7.1 Library Version

```c
/* version.h */
#define HYBRID64_VERSION_MAJOR 0
#define HYBRID64_VERSION_MINOR 1
#define HYBRID64_VERSION_PATCH 0
#define HYBRID64_VERSION_STR   "0.1.0-draft"
```

### 7.2 ABI Stability

- While `HYBRID64_VERSION_MAJOR == 0`, the ABI is **unstable**. Headers,
  struct layouts, and error codes may change between minor versions.
- ABI stability is promised starting from `v1.0.0`.

### 7.3 Spec Version vs Library Version

The specification version (`SPEC.md` front matter) and the library version
(`version.h`) are kept in sync: a `v0.2.0` library tag must correspond to a
`docs/SPEC.md` at version `0.2`.

### 7.4 Release Tagging

- Tags follow `vMAJOR.MINOR.PATCH` (e.g., `v0.1.0`).
- Tags must be GPG-signed by a maintainer key.
- Each release includes a `SHA-256` checksum file for the source archive.

---

## 8. Threat Model and Safety Requirements

### 8.1 Assets

| Asset | Description |
|---|---|
| Drive contents | Data stored in the underlying storage medium |
| Drive metadata | Offsets, lengths, flags, handle state |
| Crypto keys | Keys passed to the optional crypto layer |

### 8.2 Threat Actors

| Actor | Capabilities |
|---|---|
| Malicious caller | Passes invalid arguments, out-of-bounds offsets, crafted lengths |
| Compromised storage | Returns crafted data on read (covered by crypto verify) |
| Physical attacker on MCU | Direct flash read (mitigated by crypto layer; full mitigation is out of scope here) |

### 8.3 Safety Requirements

1. **Input validation** — Every public function must validate all pointer and
   length arguments before use. No pointer is dereferenced before a `NULL`
   check.
2. **Bounds checking** — Every read/write must check that
   `offset + len` does not overflow `uint64_t` and does not exceed the
   reported drive capacity. Return `H64_ERR_BOUNDS` on violation.
3. **Integer overflow prevention** — All arithmetic on offsets and lengths
   must be checked for overflow before use. Use `__builtin_add_overflow` or
   equivalent where available, or explicit checks otherwise.
4. **No use of uninitialized memory** — Handles must be fully zeroed by
   `hybrid64_init()` before any fields are accessed.
5. **Safe defaults** — A freshly initialised handle must refuse all I/O
   operations with `H64_ERR_NOT_OPEN` until `hybrid64_open()` succeeds.
6. **Crypto fail-closed** — If the crypto verify step fails on read,
   `hybrid64_read()` must return `H64_ERR_CRYPTO` and must not return the
   (potentially forged) plaintext data to the caller.
7. **No format-string vulnerabilities** — The log callback receives a
   pre-formatted string; the library must never pass caller-controlled data
   directly as a `printf` format string.

### 8.4 Out-of-Scope Threats

- Side-channel attacks (timing, cache, power analysis).
- Attacks requiring kernel or hypervisor compromise.
- Full-disk encryption key management (key derivation, key storage, and key
  rotation are the responsibility of the caller).

---

## 9. Build and CI Expectations

### 9.1 Compiler Requirements

- **C standard:** C11 (`-std=c11`).
- **Freestanding core:** `core/` must compile cleanly with
  `-ffreestanding -nostdlib -Wall -Wextra -Wpedantic -Werror`.
- **No compiler warnings promoted to errors are waived** without an
  accompanying comment explaining the waiver.

### 9.2 Cross-Compilation Checks

| Toolchain | Target | Checked by CI |
|---|---|---|
| `gcc` (host) | `x86_64-linux-gnu` | Yes |
| `aarch64-linux-gnu-gcc` | `aarch64-linux-gnu` | Yes |
| `arm-none-eabi-gcc` | Cortex-M (bare-metal) | Yes (stub HAL) |

A cross-compile check does **not** require running the binary; it only
requires that the build succeeds without errors or warnings.

### 9.3 Static Analysis

- **cppcheck** — run on `core/` and `platform/` with `--error-exitcode=1`.
- **clang-tidy** — run on `core/` with at minimum the `cert-*`, `clang-analyzer-*`,
  and `bugprone-*` checks enabled.
- Both tools must report zero issues for a PR to be merged.

### 9.4 Formatting

- **clang-format** — `.clang-format` config will be added in the scaffolding
  PR. All C source files must be formatted before merge.

### 9.5 CI Pipeline (planned)

```
build:
  - compile core/ for x86_64-linux-gnu
  - compile core/ for aarch64-linux-gnu
  - compile core/ for arm-none-eabi (Cortex-M, stub HAL)
  - compile platform/posix for x86_64-linux-gnu and aarch64-linux-gnu

static-analysis:
  - cppcheck core/ platform/
  - clang-tidy core/

tests (future):
  - unit tests for core/ run on host (x86_64)
  - integration tests for platform/posix run on host
```

---

## 10. Legacy 64 Migration

### 10.1 Purpose

This section tracks the migration of code from the "Legacy 64" source
repositories into the Hybrid-64 architecture.

### 10.2 Known Legacy Repos

> **TODO:** Link the Legacy 64 source repository/repositories here once URLs
> are provided by the maintainer.
>
> Example entry:
> ```
> - https://github.com/<owner>/<legacy-repo> — <brief description>
>   Migration status: [ ] Identified [ ] Mapped [ ] Ported [ ] Tested
> ```

### 10.3 Migration Principles

1. **No code is imported verbatim** unless it already satisfies the portability
   and safety rules in §2 and §8. Every imported file must be reviewed for
   dynamic allocation, libc dependencies, and undefined behaviour.
2. **Incremental migration** — functionality is migrated one module at a time
   with corresponding unit tests.
3. **Compatibility shims** — if legacy callers depend on an old API signature,
   a thin compatibility header (`legacy_compat.h`) may be provided in
   `platform/posix/` for a transitional period, to be removed before `v1.0.0`.
4. **Audit trail** — each migrated module must include a comment block noting
   the original source file, the commit or revision it was taken from, and the
   changes made during porting.

### 10.4 Migration Checklist (placeholder)

- [ ] Identify all modules in Legacy 64 repos that map to `core/`
- [ ] Identify all modules that are POSIX-specific → `platform/posix/`
- [ ] Identify all modules that are MCU-specific → `platform/mcu/`
- [ ] For each module: review for dynamic allocation and UB
- [ ] Port modules to freestanding C11
- [ ] Add unit tests for each ported module
- [ ] Remove or archive compatibility shims before `v1.0.0`

---

*End of Hybrid-64 Drive Specification v0.1-draft*
