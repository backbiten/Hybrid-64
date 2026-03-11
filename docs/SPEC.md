# Hybrid-64 Drive — Specification

**Version:** 0.1-draft  
**Date:** 2026-03-11  
**Status:** Draft — subject to change before v0.1.0 tag  
**Maintainer:** backbiten

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [API Surface](#3-api-surface)
4. [Error Model](#4-error-model)
5. [POSIX Platform Adapter](#5-posix-platform-adapter)
6. [Safety Requirements](#6-safety-requirements)
7. [Build](#7-build)

---

## 1. Overview

Hybrid-64 is a cross-platform, portable "drive" abstraction library written in
freestanding C. It provides a minimal block-oriented I/O interface — open,
close, read, write, flush — that runs identically on:

- 64-bit Linux hosts (x86-64 and arm64)
- Bare-metal or RTOS microcontrollers (ESP32, STM32, RP2040, nRF52, …)

The architectural split — portable `core/` plus thin `platform/` adapters —
allows the same safety guarantees and business logic to be shared across all
targets without conditional compilation spread throughout the library.

---

## 2. Architecture

### 2.1 Directory Layout

```
Hybrid-64/
├── core/
│   ├── include/
│   │   └── hybrid64/
│   │       ├── drive.h       # Primary drive abstraction header
│   │       ├── error.h       # Error codes
│   │       └── version.h     # Version macros
│   └── src/
│       └── drive.c           # Core drive logic (wires vtable calls)
├── platform/
│   └── posix/
│       ├── include/
│       │   └── posix_drive.h # POSIX context struct + ops declaration
│       ├── src/
│       │   └── posix_drive.c # POSIX file/block-device adapter
│       └── tools/
│           └── hybrid64.c    # Linux CLI
├── docs/
│   ├── SPEC.md               # This file
│   └── BLOCK_DEVICES.md      # Safe block-device usage guide
├── CMakeLists.txt
└── README.md
```

### 2.2 Platform Ops Vtable

The abstraction boundary between `core/` and `platform/` is
`hybrid64_platform_ops_t` (defined in `core/include/hybrid64/drive.h`):

```c
typedef struct hybrid64_platform_ops {
    int (*open) (void *ctx, const char *path, int flags);
    int (*close)(void *ctx);
    int (*read) (void *ctx, uint64_t offset, void *buf, size_t len);
    int (*write)(void *ctx, uint64_t offset, const void *buf, size_t len);
    int (*flush)(void *ctx);
} hybrid64_platform_ops_t;
```

All fields are required. Unsupported operations must return
`H64_ERR_NOT_IMPLEMENTED`.

### 2.3 Core Constraints

`core/` must compile with `-ffreestanding -nostdlib`. Permitted headers:
`<stdint.h>`, `<stddef.h>`, `<stdbool.h>`. No calls to `<stdio.h>`,
`<stdlib.h>`, `<string.h>`, or any POSIX header.

---

## 3. API Surface

```c
/* Initialise handle. 'ops' must remain valid for the lifetime of drv. */
int hybrid64_init (hybrid64_drive_t *drv,
                   const hybrid64_platform_ops_t *ops,
                   void *platform_ctx);

/* Open/close. */
int hybrid64_open (hybrid64_drive_t *drv, const char *path, int flags);
int hybrid64_close(hybrid64_drive_t *drv);

/* I/O. */
int hybrid64_read (hybrid64_drive_t *drv, uint64_t offset,
                   void *buf, size_t len);
int hybrid64_write(hybrid64_drive_t *drv, uint64_t offset,
                   const void *buf, size_t len);
int hybrid64_flush(hybrid64_drive_t *drv);

/* Error strings. */
const char *hybrid64_strerror(int err);
```

Open flags (`#define H64_FLAG_*`):

| Flag             | Meaning                              |
|------------------|--------------------------------------|
| `H64_FLAG_READ`  | Open for reading (implied)           |
| `H64_FLAG_WRITE` | Open for writing (must be explicit)  |
| `H64_FLAG_SYNC`  | Request write-through / O_SYNC       |

---

## 4. Error Model

All public API functions return `H64_OK` (0) on success or a negative error
code.

| Code                    | Value | Meaning                                   |
|-------------------------|-------|-------------------------------------------|
| `H64_OK`                | 0     | Success                                   |
| `H64_ERR_INVALID_ARG`   | -1    | NULL pointer or out-of-range argument     |
| `H64_ERR_NOT_OPEN`      | -2    | Operation on a drive that is not open     |
| `H64_ERR_IO`            | -3    | Underlying I/O failure                    |
| `H64_ERR_NOT_IMPLEMENTED`| -4   | Operation not supported by this adapter   |
| `H64_ERR_PERMISSION`    | -5    | Missing safety flags                      |
| `H64_ERR_MOUNTED`       | -6    | Device is mounted; write refused          |
| `H64_ERR_ROOT_DEVICE`   | -7    | Device is root filesystem; write refused  |
| `H64_ERR_READ_ONLY`     | -8    | Drive opened without `H64_FLAG_WRITE`     |

---

## 5. POSIX Platform Adapter

The POSIX adapter (`platform/posix/`) implements the vtable using Linux
syscalls: `open(2)`, `pread(2)`, `pwrite(2)`, `fsync(2)`, `close(2)`.

It supports:

- Regular files (any path)
- Block devices under `/dev/*`

### 5.1 Context Struct

```c
typedef struct posix_drive_ctx {
    int  fd;                            /* -1 when closed */
    char path[POSIX_DRIVE_MAX_PATH];
    int  is_block;                      /* non-zero for block devices */
    int  allow_write;                   /* must be 1 to write to /dev/* */
    int  danger_accept_data_loss;       /* must be 1 to write to /dev/* */
} posix_drive_ctx_t;
```

### 5.2 Safety Gates for Block Device Writes

When `H64_FLAG_WRITE` is set and the path starts with `/dev/`:

1. Both `allow_write` **and** `danger_accept_data_loss` must be non-zero;
   otherwise `H64_ERR_PERMISSION` is returned.
2. `/proc/mounts` is parsed; if the device is listed as mounted,
   `H64_ERR_MOUNTED` is returned.
3. If the device matches the root filesystem device (also from `/proc/mounts`),
   `H64_ERR_ROOT_DEVICE` is returned.

Checks 2 and 3 use `stat(2)` to compare `st_rdev` values so that symlinks
under `/dev/disk/by-id/` are handled correctly (best-effort).

---

## 6. Safety Requirements

- **Read-only by default**: drives are opened read-only unless
  `H64_FLAG_WRITE` is explicitly set.
- **No writes to mounted devices**: the POSIX adapter refuses writes to
  any device listed in `/proc/mounts`.
- **No writes to the root device**: guards against accidentally overwriting
  the running system.
- **Dry-run mode**: the CLI supports `--dry-run` to show what would happen
  without performing any I/O.
- **Explicit preview**: before writing, the CLI prints the target path,
  byte offset, and the first 16 bytes to be written.

For safe testing, use loop devices (`/dev/loopX`) backed by a throw-away
file. See [docs/BLOCK_DEVICES.md](BLOCK_DEVICES.md).

---

## 7. Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
# Binary: build/hybrid64
```

Requirements: CMake ≥ 3.16, GCC or Clang, Linux (for the POSIX adapter).
The `core/` library alone compiles on any C11 toolchain.
