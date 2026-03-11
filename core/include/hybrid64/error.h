/**
 * @file error.h
 * @brief Hybrid-64 error codes.
 *
 * All public API functions return H64_OK (0) on success or a negative
 * error code on failure.  The core never uses errno; platform adapters
 * map system errors to these codes.
 */
#ifndef HYBRID64_ERROR_H
#define HYBRID64_ERROR_H

#define H64_OK                   0   /**< Success */
#define H64_ERR_INVALID_ARG     -1   /**< NULL pointer or out-of-range argument */
#define H64_ERR_NOT_OPEN        -2   /**< Operation on a drive that is not open */
#define H64_ERR_IO              -3   /**< Underlying I/O failure */
#define H64_ERR_NOT_IMPLEMENTED -4   /**< Operation not supported by this adapter */
#define H64_ERR_PERMISSION      -5   /**< Caller lacks the required safety flags */
#define H64_ERR_MOUNTED         -6   /**< Device is currently mounted; write refused */
#define H64_ERR_ROOT_DEVICE     -7   /**< Device is the root filesystem; write refused */
#define H64_ERR_READ_ONLY       -8   /**< Drive was opened without H64_FLAG_WRITE */

/**
 * @brief Return a human-readable string for an error code.
 * @param err  One of the H64_* constants above.
 * @return     A static string; never NULL.
 */
const char *hybrid64_strerror(int err);

#endif /* HYBRID64_ERROR_H */
