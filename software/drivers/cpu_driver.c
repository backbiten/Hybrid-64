/**
 * cpu_driver.c — Hybrid-64 CPU driver
 *
 * Higher-level convenience wrappers around the HAL for loading programs,
 * dumping state, and running diagnostics.
 */
#include "cpu_driver.h"
#include "../hal/hal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ── Load a program from a flat array of instruction words ────────────────── */
hal_status_t cpu_load_program(hal_device_t *dev,
                              const uint32_t *instrs, size_t count)
{
    if (!dev || !instrs) return HAL_ERR_NULL;
    if (count * H64_INSTR_SIZE > H64_MEM_SIZE) return HAL_ERR_RANGE;

    hal_status_t st = hal_reset(dev);
    if (st != HAL_OK) return st;

    for (size_t i = 0; i < count; i++) {
        st = hal_mem_write_instr(dev, (uint64_t)(i * H64_INSTR_SIZE), instrs[i]);
        if (st != HAL_OK) return st;
    }
    return HAL_OK;
}

/* ── Dump all CPU registers to stdout ─────────────────────────────────────── */
void cpu_dump_regs(const hal_device_t *dev)
{
    uint64_t regs[H64_NUM_REGS];
    if (hal_reg_read_all(dev, regs) != HAL_OK) {
        fprintf(stderr, "cpu_dump_regs: failed to read registers\n");
        return;
    }

    hal_cpu_status_t status;
    hal_get_status(dev, &status);

    printf("─── Hybrid-64 Register Dump ──────────────────────────────────\n");
    printf("PC: 0x%016llx  cycles: %llu  %s%s%s%s\n",
           (unsigned long long)status.pc,
           (unsigned long long)status.cycles,
           status.halted   ? "[HALTED] "   : "",
           status.zero     ? "[Z] "        : "",
           status.carry    ? "[C] "        : "",
           status.overflow ? "[OV] "       : "");
    for (int i = 0; i < H64_NUM_REGS; i += 4) {
        printf("  r%-2d=0x%016llx  r%-2d=0x%016llx"
               "  r%-2d=0x%016llx  r%-2d=0x%016llx\n",
               i,   (unsigned long long)regs[i],
               i+1, (unsigned long long)regs[i+1],
               i+2, (unsigned long long)regs[i+2],
               i+3, (unsigned long long)regs[i+3]);
    }
    printf("──────────────────────────────────────────────────────────────\n");
}

/* ── Dump a region of memory as hex ───────────────────────────────────────── */
void cpu_dump_mem(const hal_device_t *dev, uint64_t addr, size_t len)
{
    if (!dev) return;
    uint8_t buf[16];

    printf("─── Memory @ 0x%016llx (len=%zu) ────────────────────────────\n",
           (unsigned long long)addr, len);

    for (size_t off = 0; off < len; off += 16) {
        size_t chunk = (len - off < 16) ? len - off : 16;
        if (hal_mem_read(dev, addr + off, buf, chunk) != HAL_OK) break;
        printf("  %016llx: ", (unsigned long long)(addr + off));
        for (size_t j = 0; j < 16; j++) {
            if (j < chunk) printf("%02x ", buf[j]);
            else           printf("   ");
            if (j == 7)    printf(" ");
        }
        printf(" |");
        for (size_t j = 0; j < chunk; j++)
            printf("%c", (buf[j] >= 0x20 && buf[j] < 0x7F) ? buf[j] : '.');
        printf("|\n");
    }
    printf("──────────────────────────────────────────────────────────────\n");
}

/* ── Run and report timing ────────────────────────────────────────────────── */
hal_status_t cpu_run_report(hal_device_t *dev, uint64_t max_cycles)
{
    if (!dev) return HAL_ERR_NULL;

    hal_status_t st = hal_run(dev, max_cycles);

    hal_cpu_status_t status;
    hal_get_status(dev, &status);
    printf("cpu_run_report: %llu cycles executed, PC=0x%016llx, %s\n",
           (unsigned long long)status.cycles,
           (unsigned long long)status.pc,
           status.halted ? "HALTED" : "RUNNING");
    return st;
}
