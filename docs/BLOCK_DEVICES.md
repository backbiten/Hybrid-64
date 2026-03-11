# Block Device Support in Hybrid-64

This document describes how the Hybrid-64 POSIX adapter handles Linux block
devices and explains the safety gates that prevent accidental data loss.

---

## Table of Contents

1. [Overview](#1-overview)
2. [Supported Targets](#2-supported-targets)
3. [Safety Gates](#3-safety-gates)
4. [CLI Usage](#4-cli-usage)
5. [Safe Testing with Loop Devices](#5-safe-testing-with-loop-devices)
6. [Known Limitations](#6-known-limitations)
7. [Real-Disk Usage (Advanced)](#7-real-disk-usage-advanced)

---

## 1. Overview

The POSIX adapter (`platform/posix/src/posix_drive.c`) can open both regular
files and Linux block devices such as `/dev/sda`, `/dev/nvme0n1`, or
`/dev/loop0`.

**Reads** from block devices require no special flags — the device is opened
`O_RDONLY` by default.

**Writes** to any path that starts with `/dev/` are gated behind multiple
explicit safety flags and runtime checks to prevent accidental data
destruction.

---

## 2. Supported Targets

| Path type             | Read | Write                        |
|-----------------------|------|------------------------------|
| Regular file          | ✅   | ✅ (`H64_FLAG_WRITE`)        |
| `/dev/loopX`          | ✅   | ✅ (with safety flags)       |
| `/dev/sdX`            | ✅   | ✅ (with safety flags)       |
| `/dev/nvmeXnY`        | ✅   | ✅ (with safety flags)       |
| Mounted device        | ✅   | ❌ (refused by adapter)      |
| Root filesystem device| ✅   | ❌ (refused by adapter)      |

---

## 3. Safety Gates

When a write is requested to any `/dev/*` path, the adapter performs the
following checks **before** opening the file descriptor:

### Gate 1 — Explicit opt-in flags
Two flags in `posix_drive_ctx_t` must **both** be non-zero:

- `allow_write` — confirms the caller intends to write
- `danger_accept_data_loss` — acknowledges that writing to a raw block device
  can permanently destroy data

If either flag is unset, the adapter returns `H64_ERR_PERMISSION` and prints
an explanatory message to stderr.

### Gate 2 — Mounted device check
The adapter reads `/proc/mounts` and compares device nodes using `stat(2)`
`st_rdev` values (so `/dev/disk/by-id/...` symlinks are handled correctly).
If the target device is listed as mounted, `H64_ERR_MOUNTED` is returned.

### Gate 3 — Root filesystem device check
The adapter checks whether the target is the device mounted at `/`.
If so, `H64_ERR_ROOT_DEVICE` is returned.

### Additional CLI safeguards
The `hybrid64` CLI tool adds:

- A second layer of safety-flag checking **before** calling the library (for
  a clear user-facing error message).
- An explicit **preview** of the first 16 bytes to be written and the target
  offset, printed to stderr before executing the write.
- `--dry-run` mode: logs what would happen without performing any I/O.

---

## 4. CLI Usage

### Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Subcommands

```bash
# Print type and size of a drive
./build/hybrid64 info --drive /dev/loop0
./build/hybrid64 info --drive /tmp/disk.img

# Read 512 bytes at offset 0 and save to a file
./build/hybrid64 read --drive /dev/loop0 --offset 0 --len 512 --out dump.bin

# Dry-run: show what a write would do without doing it
./build/hybrid64 write --drive /dev/loop0 --in payload.bin \
    --offset 0 --allow-write --danger-accept-data-loss --dry-run

# Actually write (requires both safety flags)
./build/hybrid64 write --drive /dev/loop0 --in payload.bin \
    --offset 0 --allow-write --danger-accept-data-loss

# Flush
./build/hybrid64 flush --drive /tmp/disk.img
```

### Attempting a write without safety flags (expected refusal)

```bash
./build/hybrid64 write --drive /dev/loop0 --in payload.bin --offset 0
# [hybrid64] ERROR: Writing to block device '/dev/loop0' requires BOTH:
# [hybrid64]   --allow-write
# [hybrid64]   --danger-accept-data-loss
```

---

## 5. Safe Testing with Loop Devices

Loop devices (`/dev/loopX`) are backed by a file on your existing filesystem.
They are the **recommended** way to develop and test block-device code because:

- You risk only the backing file, not a physical disk.
- They can be set up and torn down without root on some configurations, or
  trivially with `losetup`.

### Step-by-step

```bash
# 1. Create a 16 MiB backing file
dd if=/dev/zero of=/tmp/test_disk.img bs=1M count=16

# 2. Attach it to a loop device (requires root)
sudo losetup --find --show /tmp/test_disk.img
# Prints something like: /dev/loop7

# 3. Read the first 512 bytes (should be all zeros)
./build/hybrid64 read --drive /dev/loop7 --offset 0 --len 512 --out sector0.bin
xxd sector0.bin | head

# 4. Write a test pattern (requires both safety flags)
echo "Hello, Hybrid-64!" > /tmp/hello.bin
./build/hybrid64 write --drive /dev/loop7 --in /tmp/hello.bin \
    --offset 0 --allow-write --danger-accept-data-loss

# 5. Read it back
./build/hybrid64 read --drive /dev/loop7 --offset 0 --len 32 --out verify.bin
cat verify.bin

# 6. Detach the loop device when done
sudo losetup --detach /dev/loop7
```

---

## 6. Known Limitations

- **Best-effort mount check**: `/proc/mounts` may not list all mounts on all
  Linux configurations (e.g., some bind mounts). Do not rely on the mount
  check as the sole safeguard.
- **No partition awareness**: the adapter operates at the raw device level.
  Writing at an arbitrary offset can corrupt filesystem metadata silently.
- **No write journalling or rollback**: the adapter does not implement
  transactions; a partial write due to power loss or an error may leave the
  device in an inconsistent state.
- **pread/pwrite with `off_t`**: on 32-bit hosts, large offsets require
  `_FILE_OFFSET_BITS=64` (already set in `posix_drive.c`). The POSIX adapter
  targets 64-bit Linux; testing on 32-bit hosts is not a goal.

---

## 7. Real-Disk Usage (Advanced)

> ⚠️ **WARNING**: Writing to a real disk (`/dev/sda`, `/dev/nvme0n1`, etc.)
> will permanently destroy data on that disk. There is no undo. Triple-check
> the target path before proceeding.

Requirements:

1. The target device must **not** be mounted anywhere.
2. You must run with sufficient privileges (typically `root`).
3. Both `--allow-write` and `--danger-accept-data-loss` must be passed to the
   CLI.
4. Always do a `--dry-run` first to confirm the correct target and offsets.

Example (use at your own risk):

```bash
# Confirm target is correct and unmounted
sudo umount /dev/sdb    # if necessary
lsblk /dev/sdb

# Dry-run first
sudo ./build/hybrid64 write --drive /dev/sdb --in image.bin \
    --offset 0 --allow-write --danger-accept-data-loss --dry-run

# If the dry-run output looks correct, remove --dry-run to execute
sudo ./build/hybrid64 write --drive /dev/sdb --in image.bin \
    --offset 0 --allow-write --danger-accept-data-loss
```
