/*
 * hybrid64.c — Linux CLI wrapper for the Hybrid-64 block-device library.
 *
 * Subcommands:
 *   info   --drive PATH
 *   read   --drive PATH --offset N --len N [--out FILE]
 *   write  --drive PATH --offset N [--in FILE]
 *   flush  --drive PATH
 *
 * Safety for writes to /dev/[...] block devices:
 *   BOTH --allow-write AND --danger-accept-data-loss must be passed, or
 *   the write is refused before any device is opened.
 *
 * All diagnostics go to stderr with the "[hybrid64]" prefix.
 * Binary data output goes to stdout (or --out FILE).
 *
 * Build:  (see platform/posix/CMakeLists.txt)
 * Usage:  hybrid64 --help
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

#ifdef __linux__
#  include <sys/ioctl.h>
#  include <linux/fs.h>
#endif

#include "hybrid64/drive.h"
#include "hybrid64/error.h"
#include "hybrid64/version.h"
#include "posix_drive.h"

/* -----------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------- */

#define TOOL_NAME          "hybrid64"
#define MAX_PREVIEW_BYTES  16u
#define DEFAULT_READ_LEN   512u
#define STDIN_CHUNK_SIZE   65536u

/* -----------------------------------------------------------------------
 * Usage
 * --------------------------------------------------------------------- */

static void print_usage(void)
{
    fprintf(stderr,
        "Usage: %s <subcommand> [options]\n"
        "\n"
        "Subcommands:\n"
        "  info   --drive PATH\n"
        "         Print device type (file / block device) and size.\n"
        "\n"
        "  read   --drive PATH [--offset N] [--len N] [--out FILE]\n"
        "         Read bytes from PATH.  Defaults: offset=0, len=%u.\n"
        "         Output goes to FILE, or stdout if --out is omitted.\n"
        "\n"
        "  write  --drive PATH [--offset N] [--in FILE]\n"
        "         Write bytes to PATH.  Input comes from FILE or stdin.\n"
        "         Writing to /dev/* requires BOTH:\n"
        "           --allow-write\n"
        "           --danger-accept-data-loss\n"
        "\n"
        "  flush  --drive PATH\n"
        "         Flush/sync pending writes to storage.\n"
        "\n"
        "Options:\n"
        "  --allow-write              Enable write mode.\n"
        "  --danger-accept-data-loss  Acknowledge permanent data-loss risk.\n"
        "  --dry-run                  Show what would be done; do not execute.\n"
        "  --version                  Print version and exit.\n"
        "  --help, -h                 Print this help and exit.\n",
        TOOL_NAME, DEFAULT_READ_LEN);
}

/* -----------------------------------------------------------------------
 * 'info' subcommand
 * --------------------------------------------------------------------- */

static int cmd_info(const char *drive_path)
{
    /* Open first, then fstat() on the fd to eliminate the TOCTOU race
     * that would arise from stat() + open() on the same path. */
    int fd = open(drive_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "[hybrid64] ERROR: open('%s'): %s\n",
                drive_path, strerror(errno));
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        fprintf(stderr, "[hybrid64] ERROR: fstat('%s'): %s\n",
                drive_path, strerror(errno));
        close(fd);
        return 1;
    }

    if (S_ISREG(st.st_mode)) {
        fprintf(stderr, "[hybrid64] info: '%s'  type=regular-file  "
                "size=%llu bytes\n",
                drive_path, (unsigned long long)st.st_size);

    } else if (S_ISBLK(st.st_mode)) {
        long long size = -1;

#if defined(__linux__) && defined(BLKGETSIZE64)
        {
            uint64_t sz = 0;
            if (ioctl(fd, BLKGETSIZE64, &sz) == 0)
                size = (long long)sz;
        }
#endif
        if (size >= 0)
            fprintf(stderr, "[hybrid64] info: '%s'  type=block-device  "
                    "size=%lld bytes\n", drive_path, size);
        else
            fprintf(stderr, "[hybrid64] info: '%s'  type=block-device  "
                    "size=unknown\n", drive_path);
    } else {
        fprintf(stderr, "[hybrid64] info: '%s'  type=other\n", drive_path);
    }

    close(fd);
    return 0;
}

/* -----------------------------------------------------------------------
 * Read input data into a dynamically-grown buffer.
 * Works for both seekable files and pipes/stdin.
 * --------------------------------------------------------------------- */

static unsigned char *read_all(FILE *in, size_t *out_len)
{
    size_t cap  = STDIN_CHUNK_SIZE;
    size_t used = 0;

    unsigned char *buf = (unsigned char *)malloc(cap);
    if (!buf) return NULL;

    size_t chunk;
    while ((chunk = fread(buf + used, 1, cap - used, in)) > 0) {
        used += chunk;
        if (used == cap) {
            size_t         new_cap = cap * 2;
            unsigned char *tmp     = (unsigned char *)realloc(buf, new_cap);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
            cap = new_cap;
        }
    }

    if (ferror(in)) { free(buf); return NULL; }
    *out_len = used;
    return buf;
}

/* -----------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    if (argc < 2) { print_usage(); return 1; }

    if (strcmp(argv[1], "--version") == 0) {
        printf("%s %s\n", TOOL_NAME, HYBRID64_VERSION_STRING);
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage();
        return 0;
    }

    const char *subcmd    = argv[1];
    const char *drive_path = NULL;
    const char *out_file   = NULL;
    const char *in_file    = NULL;
    uint64_t    offset     = 0;
    size_t      len        = DEFAULT_READ_LEN;
    int         allow_write = 0;
    int         danger_flag = 0;
    int         dry_run     = 0;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--drive") == 0 && i + 1 < argc) {
            drive_path = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (strcmp(argv[i], "--in") == 0 && i + 1 < argc) {
            in_file = argv[++i];
        } else if (strcmp(argv[i], "--offset") == 0 && i + 1 < argc) {
            offset = (uint64_t)strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--len") == 0 && i + 1 < argc) {
            len = (size_t)strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--allow-write") == 0) {
            allow_write = 1;
        } else if (strcmp(argv[i], "--danger-accept-data-loss") == 0) {
            danger_flag = 1;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
        } else {
            fprintf(stderr, "[hybrid64] WARNING: unknown option '%s'\n", argv[i]);
        }
    }

    if (!drive_path) {
        fprintf(stderr, "[hybrid64] ERROR: --drive PATH is required\n");
        print_usage();
        return 1;
    }

    /* ------------------------------------------------------------------ */
    /* info                                                                 */
    /* ------------------------------------------------------------------ */
    if (strcmp(subcmd, "info") == 0) {
        return cmd_info(drive_path);
    }

    /* ------------------------------------------------------------------ */
    /* read                                                                 */
    /* ------------------------------------------------------------------ */
    if (strcmp(subcmd, "read") == 0) {
        fprintf(stderr,
                "[hybrid64] read: drive='%s' offset=%llu len=%zu\n",
                drive_path, (unsigned long long)offset, len);

        if (dry_run) {
            fprintf(stderr,
                "[hybrid64] DRY-RUN: would read %zu bytes at offset %llu "
                "from '%s'\n",
                len, (unsigned long long)offset, drive_path);
            return 0;
        }

        posix_drive_ctx_t ctx = {
            .fd                      = -1,
            .allow_write             = 0,
            .danger_accept_data_loss = 0,
        };
        hybrid64_drive_t drv;
        hybrid64_init(&drv, &posix_drive_ops, &ctx);

        int r = hybrid64_open(&drv, drive_path, H64_FLAG_READ);
        if (r != H64_OK) {
            fprintf(stderr, "[hybrid64] ERROR: open failed: %s\n",
                    hybrid64_strerror(r));
            return 1;
        }

        unsigned char *buf = (unsigned char *)malloc(len);
        if (!buf) {
            fprintf(stderr, "[hybrid64] ERROR: out of memory\n");
            hybrid64_close(&drv);
            return 1;
        }

        int n = hybrid64_read(&drv, offset, buf, len);
        if (n < 0) {
            fprintf(stderr, "[hybrid64] ERROR: read failed: %s\n",
                    hybrid64_strerror(n));
            free(buf);
            hybrid64_close(&drv);
            return 1;
        }

        FILE *out = stdout;
        if (out_file) {
            out = fopen(out_file, "wb");
            if (!out) {
                fprintf(stderr, "[hybrid64] ERROR: open output '%s': %s\n",
                        out_file, strerror(errno));
                free(buf);
                hybrid64_close(&drv);
                return 1;
            }
        }

        /* Ensure all bytes are written and detect any write errors. */
        size_t total_written = 0;
        size_t to_write = (size_t)n;
        while (total_written < to_write) {
            size_t written = fwrite(buf + total_written, 1,
                                    to_write - total_written, out);
            if (written == 0) {
                if (ferror(out)) {
                    fprintf(stderr,
                            "[hybrid64] ERROR: write to %s failed: %s\n",
                            out_file ? out_file : "stdout", strerror(errno));
                } else {
                    fprintf(stderr,
                            "[hybrid64] ERROR: write to %s incomplete\n",
                            out_file ? out_file : "stdout");
                }
                if (out_file) fclose(out);
                free(buf);
                hybrid64_close(&drv);
                return 1;
            }
            total_written += written;
        }

        if (fflush(out) == EOF) {
            fprintf(stderr,
                    "[hybrid64] ERROR: flush to %s failed: %s\n",
                    out_file ? out_file : "stdout", strerror(errno));
            if (out_file) fclose(out);
            free(buf);
            hybrid64_close(&drv);
            return 1;
        }
        if (out_file) fclose(out);

        free(buf);
        hybrid64_close(&drv);
        fprintf(stderr, "[hybrid64] read: %d byte(s) written to %s\n",
                n, out_file ? out_file : "stdout");
        return 0;
    }

    /* ------------------------------------------------------------------ */
    /* write                                                                */
    /* ------------------------------------------------------------------ */
    if (strcmp(subcmd, "write") == 0) {
        int is_dev = (strncmp(drive_path, "/dev/", 5) == 0);

        /* CLI-level safety gate for block devices (defence-in-depth;
         * the adapter also checks, but we want a clear user-facing error
         * before we even try to read the input file). */
        if (is_dev && (!allow_write || !danger_flag)) {
            fprintf(stderr,
                "[hybrid64] ERROR: Writing to block device '%s' requires "
                "BOTH:\n"
                "[hybrid64]   --allow-write\n"
                "[hybrid64]   --danger-accept-data-loss\n"
                "[hybrid64] These flags acknowledge that this operation can "
                "permanently destroy data.\n",
                drive_path);
            return 1;
        }

        /* Read input data. */
        FILE *in = stdin;
        if (in_file) {
            in = fopen(in_file, "rb");
            if (!in) {
                fprintf(stderr, "[hybrid64] ERROR: open input '%s': %s\n",
                        in_file, strerror(errno));
                return 1;
            }
        }

        size_t         nread = 0;
        unsigned char *buf   = read_all(in, &nread);
        if (in_file) fclose(in);

        if (!buf) {
            fprintf(stderr, "[hybrid64] ERROR: failed to read input data\n");
            return 1;
        }
        if (nread == 0) {
            fprintf(stderr, "[hybrid64] ERROR: input is empty\n");
            free(buf);
            return 1;
        }

        /* Print explicit warning with data preview. */
        fprintf(stderr,
                "[hybrid64] WARNING: About to write %zu byte(s) to '%s' "
                "at offset %llu\n",
                nread, drive_path, (unsigned long long)offset);

        size_t preview = nread < MAX_PREVIEW_BYTES ? nread : MAX_PREVIEW_BYTES;
        fprintf(stderr, "[hybrid64] First %zu byte(s) to be written:", preview);
        for (size_t i = 0; i < preview; i++)
            fprintf(stderr, " %02x", buf[i]);
        fprintf(stderr, "\n");

        if (dry_run) {
            fprintf(stderr, "[hybrid64] DRY-RUN: write not performed.\n");
            free(buf);
            return 0;
        }

        posix_drive_ctx_t ctx = {
            .fd                      = -1,
            .allow_write             = allow_write,
            .danger_accept_data_loss = danger_flag,
        };
        hybrid64_drive_t drv;
        hybrid64_init(&drv, &posix_drive_ops, &ctx);

        int r = hybrid64_open(&drv, drive_path, H64_FLAG_READ | H64_FLAG_WRITE);
        if (r != H64_OK) {
            fprintf(stderr, "[hybrid64] ERROR: open failed: %s\n",
                    hybrid64_strerror(r));
            free(buf);
            return 1;
        }

        int n = hybrid64_write(&drv, offset, buf, nread);
        free(buf);

        if (n < 0) {
            fprintf(stderr, "[hybrid64] ERROR: write failed: %s\n",
                    hybrid64_strerror(n));
            hybrid64_close(&drv);
            return 1;
        }

        hybrid64_close(&drv);
        fprintf(stderr, "[hybrid64] write: %d byte(s) written to '%s'\n",
                n, drive_path);
        return 0;
    }

    /* ------------------------------------------------------------------ */
    /* flush                                                                */
    /* ------------------------------------------------------------------ */
    if (strcmp(subcmd, "flush") == 0) {
        fprintf(stderr, "[hybrid64] flush: drive='%s'\n", drive_path);

        if (dry_run) {
            fprintf(stderr, "[hybrid64] DRY-RUN: would flush '%s'\n",
                    drive_path);
            return 0;
        }

        posix_drive_ctx_t ctx = {
            .fd                      = -1,
            .allow_write             = 0,
            .danger_accept_data_loss = 0,
        };
        hybrid64_drive_t drv;
        hybrid64_init(&drv, &posix_drive_ops, &ctx);

        /* Open read-only; fsync(2) works on read-only fds on Linux. */
        int r = hybrid64_open(&drv, drive_path, H64_FLAG_READ);
        if (r != H64_OK) {
            fprintf(stderr, "[hybrid64] ERROR: open failed: %s\n",
                    hybrid64_strerror(r));
            return 1;
        }

        r = hybrid64_flush(&drv);
        hybrid64_close(&drv);

        if (r != H64_OK) {
            fprintf(stderr, "[hybrid64] ERROR: flush failed: %s\n",
                    hybrid64_strerror(r));
            return 1;
        }

        fprintf(stderr, "[hybrid64] flush: complete\n");
        return 0;
    }

    /* ------------------------------------------------------------------ */
    /* Unknown subcommand                                                   */
    /* ------------------------------------------------------------------ */
    fprintf(stderr, "[hybrid64] ERROR: unknown subcommand '%s'\n", subcmd);
    print_usage();
    return 1;
}
