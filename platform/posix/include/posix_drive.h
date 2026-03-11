/**
 * @file posix_drive.h
 * @brief POSIX platform adapter for Hybrid-64.
 *
 * Implements hybrid64_platform_ops_t using Linux/POSIX syscalls
 * (open, pread, pwrite, fsync, close).
 *
 * Supports both regular files and block devices under /dev/[...].
 *
 * Safety contract for writes to block devices:
 *   Both posix_drive_ctx_t::allow_write AND
 *   posix_drive_ctx_t::danger_accept_data_loss must be non-zero, or
 *   posix_drive_open() will refuse and return H64_ERR_PERMISSION.
 *
 * Additional best-effort guards:
 *   - Refuse if the target is listed in /proc/mounts (H64_ERR_MOUNTED).
 *   - Refuse if the target is the root filesystem device (H64_ERR_ROOT_DEVICE).
 */
#ifndef POSIX_DRIVE_H
#define POSIX_DRIVE_H

#include "hybrid64/drive.h"

/** Maximum path length stored in the context struct. */
#define POSIX_DRIVE_MAX_PATH 4096

/**
 * @brief POSIX adapter context.
 *
 * Allocate one of these (statically or on the stack) and pass a pointer
 * to hybrid64_init() as the @p platform_ctx argument.
 *
 * @code
 *   posix_drive_ctx_t ctx = {
 *       .fd                       = -1,
 *       .allow_write              = 0,
 *       .danger_accept_data_loss  = 0,
 *   };
 *   hybrid64_drive_t drv;
 *   hybrid64_init(&drv, &posix_drive_ops, &ctx);
 * @endcode
 */
typedef struct posix_drive_ctx {
    int  fd;                            /**< File descriptor; -1 when closed. */
    char path[POSIX_DRIVE_MAX_PATH];    /**< Path as passed to open. */
    int  is_block;                      /**< Non-zero if target is a block device. */

    /**
     * Set to non-zero to permit write mode.
     * Required (together with danger_accept_data_loss) when writing to
     * any /dev/[...] path.
     */
    int  allow_write;

    /**
     * Set to non-zero to acknowledge that writes to a block device may
     * permanently destroy data.
     * Required (together with allow_write) when writing to any /dev/[...] path.
     */
    int  danger_accept_data_loss;
} posix_drive_ctx_t;

/** Singleton vtable; pass to hybrid64_init(). */
extern const hybrid64_platform_ops_t posix_drive_ops;

/* Individual vtable entry points (also callable directly if needed). */
int posix_drive_open (void *ctx, const char *path, int flags);
int posix_drive_close(void *ctx);
int posix_drive_read (void *ctx, uint64_t offset, void *buf, size_t len);
int posix_drive_write(void *ctx, uint64_t offset, const void *buf, size_t len);
int posix_drive_flush(void *ctx);

#endif /* POSIX_DRIVE_H */
