/*
 * posix_drive.c — POSIX (Linux) platform adapter for Hybrid-64.
 *
 * Uses: open(2), pread(2), pwrite(2), fsync(2), close(2).
 * Supports regular files and block devices under /dev/[...].
 *
 * Safety for writes to /dev/[...] block devices:
 *   1. Requires posix_drive_ctx_t::allow_write AND
 *      posix_drive_ctx_t::danger_accept_data_loss to be set.
 *   2. Refuses if the device is listed as mounted in /proc/mounts.
 *   3. Refuses if the device matches the root filesystem device.
 *
 * All diagnostics go to stderr with the "[hybrid64]" prefix.
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "posix_drive.h"
#include "hybrid64/error.h"

/* -----------------------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------------------- */

/** Return non-zero if @p path refers to a block device. */
static int path_is_block_device(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISBLK(st.st_mode);
}

/**
 * Compare two paths by device identity rather than string equality.
 *
 * For block devices the kernel rdev (major:minor) is compared; this
 * handles symlinks such as /dev/disk/by-id/... transparently.
 * For regular files the dev+ino pair is used.
 *
 * Returns non-zero if @p a and @p b refer to the same storage object.
 */
static int paths_same_device(const char *a, const char *b)
{
    struct stat sa, sb;
    if (stat(a, &sa) != 0 || stat(b, &sb) != 0) return 0;

    if (S_ISBLK(sa.st_mode) && S_ISBLK(sb.st_mode))
        return sa.st_rdev == sb.st_rdev;

    return (sa.st_dev == sb.st_dev) && (sa.st_ino == sb.st_ino);
}

/**
 * Parse /proc/mounts and return non-zero if @p path appears as a
 * mounted block device.
 *
 * This is best-effort: if /proc/mounts cannot be read the check is
 * skipped (returns 0).
 */
static int is_device_mounted(const char *path)
{
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return 0;

    char line[512];
    char dev[256];
    char mnt[256];
    int  found = 0;

    while (!found && fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255s %255s", dev, mnt) == 2) {
            if (paths_same_device(path, dev))
                found = 1;
        }
    }
    fclose(f);
    return found;
}

/**
 * Parse /proc/mounts and return non-zero if @p path is the device
 * mounted at "/".
 */
static int is_root_device(const char *path)
{
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return 0;

    char line[512];
    char dev[256];
    char mnt[256];
    char root_dev[256] = "";

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%255s %255s", dev, mnt) == 2) {
            if (strcmp(mnt, "/") == 0) {
                strncpy(root_dev, dev, sizeof(root_dev) - 1);
                root_dev[sizeof(root_dev) - 1] = '\0';
                break;
            }
        }
    }
    fclose(f);

    if (root_dev[0] == '\0') return 0;
    return paths_same_device(path, root_dev);
}

/* -----------------------------------------------------------------------
 * vtable implementations
 * --------------------------------------------------------------------- */

int posix_drive_open(void *ctx, const char *path, int flags)
{
    posix_drive_ctx_t *c = (posix_drive_ctx_t *)ctx;
    if (!c || !path) return H64_ERR_INVALID_ARG;

    int write_mode = (flags & H64_FLAG_WRITE) != 0;
    int is_dev     = (strncmp(path, "/dev/", 5) == 0);

    /* ---- Safety gates for writes to block devices ---- */
    if (write_mode && is_dev) {
        if (!c->allow_write || !c->danger_accept_data_loss) {
            fprintf(stderr,
                "[hybrid64] ERROR: Write to block device '%s' refused.\n"
                "[hybrid64]   Both 'allow_write' and 'danger_accept_data_loss' "
                "flags must be explicitly set to write to a block device.\n",
                path);
            return H64_ERR_PERMISSION;
        }

        if (is_device_mounted(path)) {
            fprintf(stderr,
                "[hybrid64] ERROR: Device '%s' is currently mounted. "
                "Refusing write to avoid data corruption.\n", path);
            return H64_ERR_MOUNTED;
        }

        if (is_root_device(path)) {
            fprintf(stderr,
                "[hybrid64] ERROR: Device '%s' appears to be the root "
                "filesystem device. Refusing write.\n", path);
            return H64_ERR_ROOT_DEVICE;
        }
    }

    /* ---- Open the file descriptor ---- */
    int oflags = write_mode ? O_RDWR : O_RDONLY;
    if (flags & H64_FLAG_SYNC) oflags |= O_SYNC;

    c->fd = open(path, oflags);
    if (c->fd < 0) {
        fprintf(stderr, "[hybrid64] ERROR: open('%s'): %s\n",
                path, strerror(errno));
        return H64_ERR_IO;
    }

    strncpy(c->path, path, sizeof(c->path) - 1);
    c->path[sizeof(c->path) - 1] = '\0';
    c->is_block = path_is_block_device(path);

    fprintf(stderr, "[hybrid64] opened '%s' (%s, %s)\n",
            path,
            c->is_block ? "block device" : "regular file",
            write_mode  ? "read-write"   : "read-only");
    return H64_OK;
}

int posix_drive_close(void *ctx)
{
    posix_drive_ctx_t *c = (posix_drive_ctx_t *)ctx;
    if (!c || c->fd < 0) return H64_ERR_INVALID_ARG;

    if (close(c->fd) != 0) {
        fprintf(stderr, "[hybrid64] ERROR: close: %s\n", strerror(errno));
        return H64_ERR_IO;
    }
    c->fd = -1;
    return H64_OK;
}

int posix_drive_read(void *ctx, uint64_t offset, void *buf, size_t len)
{
    posix_drive_ctx_t *c = (posix_drive_ctx_t *)ctx;
    if (!c || c->fd < 0 || !buf) return H64_ERR_INVALID_ARG;

    /* _FILE_OFFSET_BITS=64 guarantees off_t is 64-bit on Linux; guard
     * against values that would overflow the signed off_t range. */
    if (offset > (uint64_t)INT64_MAX) {
        fprintf(stderr, "[hybrid64] ERROR: offset %llu exceeds maximum "
                "supported value\n", (unsigned long long)offset);
        return H64_ERR_INVALID_ARG;
    }

    ssize_t r = pread(c->fd, buf, len, (off_t)offset);
    if (r < 0) {
        fprintf(stderr, "[hybrid64] ERROR: pread: %s\n", strerror(errno));
        return H64_ERR_IO;
    }
    return (int)r;
}

int posix_drive_write(void *ctx, uint64_t offset, const void *buf, size_t len)
{
    posix_drive_ctx_t *c = (posix_drive_ctx_t *)ctx;
    if (!c || c->fd < 0 || !buf) return H64_ERR_INVALID_ARG;

    /* Guard against off_t overflow (see posix_drive_read). */
    if (offset > (uint64_t)INT64_MAX) {
        fprintf(stderr, "[hybrid64] ERROR: offset %llu exceeds maximum "
                "supported value\n", (unsigned long long)offset);
        return H64_ERR_INVALID_ARG;
    }

    ssize_t r = pwrite(c->fd, buf, len, (off_t)offset);
    if (r < 0) {
        fprintf(stderr, "[hybrid64] ERROR: pwrite: %s\n", strerror(errno));
        return H64_ERR_IO;
    }
    return (int)r;
}

int posix_drive_flush(void *ctx)
{
    posix_drive_ctx_t *c = (posix_drive_ctx_t *)ctx;
    if (!c || c->fd < 0) return H64_ERR_INVALID_ARG;

    if (fsync(c->fd) != 0) {
        fprintf(stderr, "[hybrid64] ERROR: fsync: %s\n", strerror(errno));
        return H64_ERR_IO;
    }
    return H64_OK;
}

/* -----------------------------------------------------------------------
 * Singleton vtable
 * --------------------------------------------------------------------- */

const hybrid64_platform_ops_t posix_drive_ops = {
    .open  = posix_drive_open,
    .close = posix_drive_close,
    .read  = posix_drive_read,
    .write = posix_drive_write,
    .flush = posix_drive_flush,
};
