/*
 * drive.c — Core drive abstraction: wires caller-supplied platform vtable.
 *
 * Compiles under -ffreestanding -nostdlib (C11 freestanding subset).
 * Only <stdint.h>, <stddef.h>, <stdbool.h> are used transitively via
 * the public headers.
 */
#include "hybrid64/drive.h"
#include "hybrid64/error.h"

/* -----------------------------------------------------------------------
 * Lifecycle
 * --------------------------------------------------------------------- */

int hybrid64_init(hybrid64_drive_t              *drv,
                  const hybrid64_platform_ops_t *ops,
                  void                          *platform_ctx)
{
    if (!drv || !ops) return H64_ERR_INVALID_ARG;
    drv->ops          = ops;
    drv->platform_ctx = platform_ctx;
    drv->flags        = 0;
    drv->is_open      = false;
    return H64_OK;
}

int hybrid64_open(hybrid64_drive_t *drv, const char *path, int flags)
{
    if (!drv || !path)          return H64_ERR_INVALID_ARG;
    if (!drv->ops)              return H64_ERR_NOT_IMPLEMENTED;
    if (!drv->ops->open)        return H64_ERR_NOT_IMPLEMENTED;

    int eff_flags = flags | H64_FLAG_READ;
    int r = drv->ops->open(drv->platform_ctx, path, eff_flags);
    if (r == H64_OK) {
        drv->flags   = eff_flags;
        drv->is_open = true;
    }
    return r;
}

int hybrid64_close(hybrid64_drive_t *drv)
{
    if (!drv)               return H64_ERR_INVALID_ARG;
    if (!drv->is_open)      return H64_OK;      /* no-op if already closed */
    if (!drv->ops)          return H64_ERR_NOT_IMPLEMENTED;
    if (!drv->ops->close)   return H64_ERR_NOT_IMPLEMENTED;

    int r = drv->ops->close(drv->platform_ctx);
    if (r == H64_OK) drv->is_open = false;
    return r;
}

/* -----------------------------------------------------------------------
 * I/O
 * --------------------------------------------------------------------- */

int hybrid64_read(hybrid64_drive_t *drv,
                  uint64_t          offset,
                  void             *buf,
                  size_t            len)
{
    if (!drv || !buf)       return H64_ERR_INVALID_ARG;
    if (!drv->is_open)      return H64_ERR_NOT_OPEN;
    if (!drv->ops)          return H64_ERR_NOT_IMPLEMENTED;
    if (!drv->ops->read)    return H64_ERR_NOT_IMPLEMENTED;

    return drv->ops->read(drv->platform_ctx, offset, buf, len);
}

int hybrid64_write(hybrid64_drive_t *drv,
                   uint64_t          offset,
                   const void       *buf,
                   size_t            len)
{
    if (!drv || !buf)                       return H64_ERR_INVALID_ARG;
    if (!drv->is_open)                      return H64_ERR_NOT_OPEN;
    if (!(drv->flags & H64_FLAG_WRITE))     return H64_ERR_READ_ONLY;
    if (!drv->ops)                          return H64_ERR_NOT_IMPLEMENTED;
    if (!drv->ops->write)                   return H64_ERR_NOT_IMPLEMENTED;

    return drv->ops->write(drv->platform_ctx, offset, buf, len);
}

int hybrid64_flush(hybrid64_drive_t *drv)
{
    if (!drv)               return H64_ERR_INVALID_ARG;
    if (!drv->is_open)      return H64_ERR_NOT_OPEN;
    if (!drv->ops)          return H64_ERR_NOT_IMPLEMENTED;
    if (!drv->ops->flush)   return H64_ERR_NOT_IMPLEMENTED;

    return drv->ops->flush(drv->platform_ctx);
}

/* -----------------------------------------------------------------------
 * Error strings — kept in core so it works on every platform.
 * --------------------------------------------------------------------- */

const char *hybrid64_strerror(int err)
{
    switch (err) {
    case H64_OK:                   return "OK";
    case H64_ERR_INVALID_ARG:      return "invalid argument";
    case H64_ERR_NOT_OPEN:         return "drive not open";
    case H64_ERR_IO:               return "I/O error";
    case H64_ERR_NOT_IMPLEMENTED:  return "not implemented";
    case H64_ERR_PERMISSION:       return "permission denied (missing safety flags)";
    case H64_ERR_MOUNTED:          return "device is currently mounted";
    case H64_ERR_ROOT_DEVICE:      return "device is the root filesystem device";
    case H64_ERR_READ_ONLY:        return "drive is read-only (H64_FLAG_WRITE not set)";
    default:                       return "unknown error";
    }
}
