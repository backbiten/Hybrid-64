/**
 * @file drive.h
 * @brief Hybrid-64 primary drive abstraction header.
 *
 * This header defines the platform-operations vtable and the opaque
 * drive handle used by every layer of the library.  It must compile
 * cleanly under `-ffreestanding -nostdlib` (C11 freestanding subset:
 * <stdint.h>, <stddef.h>, <stdbool.h>).
 */
#ifndef HYBRID64_DRIVE_H
#define HYBRID64_DRIVE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -----------------------------------------------------------------------
 * Open-mode flags (bitmask passed to hybrid64_open / platform open)
 * --------------------------------------------------------------------- */

/** Open for reading. */
#define H64_FLAG_READ   (1 << 0)

/** Open for writing.  Must be set explicitly; drives are read-only by
 *  default.  Platform adapters MAY impose additional requirements (e.g.
 *  extra safety flags) before honouring this flag on block devices. */
#define H64_FLAG_WRITE  (1 << 1)

/** Request write-through / O_SYNC semantics if the platform supports it. */
#define H64_FLAG_SYNC   (1 << 2)

/* -----------------------------------------------------------------------
 * Platform operations vtable
 * --------------------------------------------------------------------- */

/**
 * @brief Function-pointer table that a platform adapter must fill.
 *
 * All functions receive a @p ctx pointer that the platform adapter uses
 * to store its own state (e.g. a file descriptor, device path, safety
 * flags).  The @p ctx pointer is supplied by the caller at
 * hybrid64_init() time and is never touched by the core.
 *
 * Return values:
 *  - open/close/flush: H64_OK (0) on success, negative H64_ERR_* on error.
 *  - read/write: number of bytes transferred (>= 0) on success, negative
 *    H64_ERR_* on error.
 *
 * A platform that does not support a particular operation MUST supply a
 * stub that returns H64_ERR_NOT_IMPLEMENTED.
 */
typedef struct hybrid64_platform_ops {
    /** Open the underlying storage at @p path with the given @p flags
     *  (bitmask of H64_FLAG_*).  Returns H64_OK or an error code. */
    int (*open)(void *ctx, const char *path, int flags);

    /** Close the underlying storage.  Returns H64_OK or an error code. */
    int (*close)(void *ctx);

    /**
     * Read up to @p len bytes at byte offset @p offset into @p buf.
     *
     * Implementations MAY return fewer than @p len bytes in a single call
     * (for example at end-of-file or if the underlying platform returns a
     * short read).  Callers that need to obtain exactly @p len bytes must
     * loop until the requested length has been read or an error/EOF is
     * encountered.
     *
     * @return  Bytes read (>= 0, possibly less than @p len) or a negative
     *          H64_ERR_* error code.
     */
    int (*read)(void *ctx, uint64_t offset, void *buf, size_t len);

    /**
     * Write up to @p len bytes at byte offset @p offset from @p buf.
     *
     * Implementations MAY write fewer than @p len bytes in a single call
     * if the underlying platform performs a short write.  Callers that
     * require all-or-nothing semantics must loop until @p len bytes have
     * been written or an error is returned.
     *
     * @return  Bytes written (>= 0, possibly less than @p len) or a
     *          negative H64_ERR_* error code.
     */
    int (*write)(void *ctx, uint64_t offset, const void *buf, size_t len);

    /** Flush/sync pending writes to persistent storage.
     *  Returns H64_OK or an error code. */
    int (*flush)(void *ctx);
} hybrid64_platform_ops_t;

/* -----------------------------------------------------------------------
 * Drive handle
 * --------------------------------------------------------------------- */

/**
 * @brief Opaque drive handle.
 *
 * Callers may allocate this statically:
 * @code
 *   hybrid64_drive_t drv;
 *   hybrid64_init(&drv, &my_ops, &my_ctx);
 * @endcode
 *
 * The struct is deliberately exposed so that callers can allocate it
 * without dynamic memory.  Do not access its fields directly.
 */
typedef struct hybrid64_drive {
    const hybrid64_platform_ops_t *ops;  /**< Platform vtable (not owned). */
    void                          *platform_ctx; /**< Adapter-private context. */
    int                            flags;        /**< Open flags (H64_FLAG_*). */
    bool                           is_open;      /**< True after a successful open. */
} hybrid64_drive_t;

/* -----------------------------------------------------------------------
 * Drive lifecycle API
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a drive handle.
 *
 * Must be called before any other function on @p drv.  Does not open
 * any underlying storage.
 *
 * @param drv           Drive handle to initialise.  Must not be NULL.
 * @param ops           Platform vtable.  Must not be NULL; must remain
 *                      valid for the lifetime of @p drv.
 * @param platform_ctx  Adapter-private context pointer passed verbatim
 *                      to every vtable call.  May be NULL if the adapter
 *                      does not need it.
 * @return H64_OK on success, H64_ERR_INVALID_ARG if @p drv or @p ops
 *         is NULL.
 */
int hybrid64_init(hybrid64_drive_t              *drv,
                  const hybrid64_platform_ops_t *ops,
                  void                          *platform_ctx);

/**
 * @brief Open the drive at @p path.
 *
 * @param drv    An initialised drive handle.
 * @param path   Platform-specific path (file path or device node).
 * @param flags  Bitmask of H64_FLAG_*.  H64_FLAG_READ is always implied.
 * @return H64_OK on success, or a negative H64_ERR_* code.
 */
int hybrid64_open(hybrid64_drive_t *drv, const char *path, int flags);

/**
 * @brief Close an open drive.
 *
 * Calling on an already-closed handle is a no-op.
 * @return H64_OK on success, or a negative H64_ERR_* code.
 */
int hybrid64_close(hybrid64_drive_t *drv);

/**
 * @brief Read bytes from an open drive.
 *
 * @param drv     An open drive handle.
 * @param offset  Byte offset from the start of the drive.
 * @param buf     Destination buffer.  Must be at least @p len bytes.
 * @param len     Number of bytes to read.
 * @return Bytes actually read (>= 0), or a negative H64_ERR_* code.
 */
int hybrid64_read(hybrid64_drive_t *drv,
                  uint64_t          offset,
                  void             *buf,
                  size_t            len);

/**
 * @brief Write bytes to an open drive.
 *
 * The drive must have been opened with H64_FLAG_WRITE.
 *
 * @param drv     An open drive handle.
 * @param offset  Byte offset from the start of the drive.
 * @param buf     Source buffer.  Must be at least @p len bytes.
 * @param len     Number of bytes to write.
 * @return Bytes actually written (>= 0), or a negative H64_ERR_* code.
 *         Returns H64_ERR_READ_ONLY if H64_FLAG_WRITE was not set on open.
 */
int hybrid64_write(hybrid64_drive_t *drv,
                   uint64_t          offset,
                   const void       *buf,
                   size_t            len);

/**
 * @brief Flush/sync an open drive.
 *
 * @return H64_OK on success, or a negative H64_ERR_* code.
 */
int hybrid64_flush(hybrid64_drive_t *drv);

#endif /* HYBRID64_DRIVE_H */
