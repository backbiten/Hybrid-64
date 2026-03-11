/**
 * hal.h — Hybrid-64 Hardware Abstraction Layer
 *
 * Provides a platform-independent interface for interacting with the
 * Hybrid-64 processor core, whether running on real FPGA hardware or
 * through the software emulation layer.
 */
#ifndef HYBRID64_HAL_H
#define HYBRID64_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ──────────────────────────────────────────────────────────────── */
#define HAL_VERSION_MAJOR 1
#define HAL_VERSION_MINOR 0
#define HAL_VERSION_PATCH 0

/* ── Constants ────────────────────────────────────────────────────────────── */
#define H64_NUM_REGS       32          /* General-purpose registers           */
#define H64_MEM_SIZE       (64*1024)   /* Default memory size: 64 KiB         */
#define H64_WORD_SIZE      8           /* Bytes per 64-bit word               */
#define H64_INSTR_SIZE     4           /* Bytes per instruction               */

/* ── Error codes ──────────────────────────────────────────────────────────── */
typedef enum {
    HAL_OK             =  0,
    HAL_ERR_NULL       = -1,  /* NULL pointer argument                       */
    HAL_ERR_RANGE      = -2,  /* Address or index out of range               */
    HAL_ERR_ALIGN      = -3,  /* Misaligned access                           */
    HAL_ERR_HALTED     = -4,  /* CPU is already halted                       */
    HAL_ERR_RUNNING    = -5,  /* CPU is currently running                    */
    HAL_ERR_UNSUPPORTED= -6,  /* Operation not supported by backend          */
    HAL_ERR_IO         = -7,  /* I/O or hardware communication error         */
} hal_status_t;

/* ── Backend type ─────────────────────────────────────────────────────────── */
typedef enum {
    HAL_BACKEND_SOFTWARE = 0,   /* Pure-software emulator                    */
    HAL_BACKEND_FPGA     = 1,   /* FPGA hardware via JTAG/PCIe               */
    HAL_BACKEND_SIM      = 2,   /* RTL simulation (Verilog/VHDL)             */
} hal_backend_t;

/* ── CPU status flags ─────────────────────────────────────────────────────── */
typedef struct {
    bool     halted;
    bool     zero;
    bool     carry;
    bool     overflow;
    uint64_t pc;        /* Program counter                                   */
    uint64_t cycles;    /* Elapsed clock cycles                              */
} hal_cpu_status_t;

/* ── ALU operation codes (mirrors hardware encoding) ─────────────────────── */
typedef enum {
    HAL_ALU_ADD  = 0x0,
    HAL_ALU_SUB  = 0x1,
    HAL_ALU_AND  = 0x2,
    HAL_ALU_OR   = 0x3,
    HAL_ALU_XOR  = 0x4,
    HAL_ALU_NOT  = 0x5,
    HAL_ALU_SHL  = 0x6,
    HAL_ALU_SHR  = 0x7,
    HAL_ALU_SAR  = 0x8,
    HAL_ALU_MUL  = 0x9,
    HAL_ALU_DIV  = 0xA,
    HAL_ALU_MOD  = 0xB,
    HAL_ALU_CMP  = 0xC,
    HAL_ALU_NAND = 0xD,
    HAL_ALU_NOR  = 0xE,
    HAL_ALU_XNOR = 0xF,
} hal_alu_op_t;

/* ── Opaque device handle ─────────────────────────────────────────────────── */
typedef struct hal_device hal_device_t;

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/**
 * hal_open() — Allocate and initialise a HAL device handle.
 *
 * @param backend   Which backend to use.
 * @param mem_size  Size of the emulated memory in bytes (must be power-of-2,
 *                  at least 256).  Pass 0 to use H64_MEM_SIZE.
 * @param out       Populated with a newly allocated handle on success.
 * @return HAL_OK on success, negative error code otherwise.
 */
hal_status_t hal_open(hal_backend_t backend, size_t mem_size,
                      hal_device_t **out);

/**
 * hal_close() — Release all resources associated with a device handle.
 *
 * @param dev  Handle returned by hal_open().  May be NULL (no-op).
 */
void hal_close(hal_device_t *dev);

/* ── Reset / control ──────────────────────────────────────────────────────── */

/** Reset the CPU and clear all registers.  Memory contents are preserved. */
hal_status_t hal_reset(hal_device_t *dev);

/** Execute a single clock cycle.  Returns HAL_ERR_HALTED if already halted. */
hal_status_t hal_step(hal_device_t *dev);

/** Run the CPU until a HALT instruction is executed or max_cycles is reached.
 *  Pass max_cycles = 0 for unlimited execution (use with care). */
hal_status_t hal_run(hal_device_t *dev, uint64_t max_cycles);

/* ── Register access ──────────────────────────────────────────────────────── */

/** Read the value of general-purpose register @reg (0–31). */
hal_status_t hal_reg_read(const hal_device_t *dev, unsigned reg,
                          uint64_t *value);

/** Write @value to general-purpose register @reg (1–31; reg 0 is read-only). */
hal_status_t hal_reg_write(hal_device_t *dev, unsigned reg, uint64_t value);

/** Read all 32 registers into @buf (must point to at least 32 uint64_t). */
hal_status_t hal_reg_read_all(const hal_device_t *dev,
                              uint64_t buf[H64_NUM_REGS]);

/* ── Memory access ────────────────────────────────────────────────────────── */

/** Read @len bytes from device memory at @addr into @buf. */
hal_status_t hal_mem_read(const hal_device_t *dev, uint64_t addr,
                          void *buf, size_t len);

/** Write @len bytes from @buf into device memory at @addr. */
hal_status_t hal_mem_write(hal_device_t *dev, uint64_t addr,
                           const void *buf, size_t len);

/** Write a 32-bit instruction word at the given byte address. */
hal_status_t hal_mem_write_instr(hal_device_t *dev, uint64_t addr,
                                 uint32_t instr);

/** Read a 64-bit word from device memory (address must be 8-byte aligned). */
hal_status_t hal_mem_read64(const hal_device_t *dev, uint64_t addr,
                             uint64_t *value);

/** Write a 64-bit word to device memory (address must be 8-byte aligned). */
hal_status_t hal_mem_write64(hal_device_t *dev, uint64_t addr, uint64_t value);

/* ── Status ───────────────────────────────────────────────────────────────── */

/** Populate @status with the current CPU status. */
hal_status_t hal_get_status(const hal_device_t *dev, hal_cpu_status_t *status);

/** Return a human-readable string for a HAL error code. */
const char *hal_strerror(hal_status_t err);

/* ── Instruction assembly helpers ─────────────────────────────────────────── */

/** Build an R-type instruction: op rs1 rs2 → rd */
static inline uint32_t hal_mk_r(uint8_t op, uint8_t rs1,
                                 uint8_t rs2, uint8_t rd)
{
    return ((uint32_t)(op  & 0x3F) << 26) |
           ((uint32_t)(rs1 & 0x1F) << 21) |
           ((uint32_t)(rs2 & 0x1F) << 16) |
           ((uint32_t)(rd  & 0x1F) << 11);
}

/** Build an I-type instruction: op rs1, imm11 → rd  (ADDI, LD) */
static inline uint32_t hal_mk_i(uint8_t op, uint8_t rs1,
                                 uint8_t rd, int16_t imm)
{
    return ((uint32_t)(op  & 0x3F) << 26) |
           ((uint32_t)(rs1 & 0x1F) << 21) |
           ((uint32_t)(rd  & 0x1F) << 11) |
           ((uint32_t)(imm & 0x7FF));
}

/**
 * Build an RS-type instruction: op rs1, rs2, imm11
 *
 * Used for instructions that have two source registers AND an immediate:
 * ST (mem[rs1+imm] = rs2), BEQ and BNE (compare rs1 vs rs2, branch by imm).
 * rs2 is encoded at bits [20:16]; imm at bits [10:0].
 */
static inline uint32_t hal_mk_rs(uint8_t op, uint8_t rs1,
                                  uint8_t rs2, int16_t imm)
{
    return ((uint32_t)(op  & 0x3F) << 26) |
           ((uint32_t)(rs1 & 0x1F) << 21) |
           ((uint32_t)(rs2 & 0x1F) << 16) |
           ((uint32_t)(imm & 0x7FF));
}

/** HALT instruction word */
#define HAL_INSTR_HALT  ((uint32_t)0xFC000000u)

/* ── Opcode constants (mirror hardware encoding) ──────────────────────────── */
#define H64_OP_NOP   0x00u
#define H64_OP_ADD   0x01u
#define H64_OP_SUB   0x02u
#define H64_OP_AND   0x03u
#define H64_OP_OR    0x04u
#define H64_OP_XOR   0x05u
#define H64_OP_SHL   0x06u
#define H64_OP_SHR   0x07u
#define H64_OP_LD    0x08u
#define H64_OP_ST    0x09u
#define H64_OP_ADDI  0x0Au
#define H64_OP_JMP   0x0Bu
#define H64_OP_BEQ   0x0Cu
#define H64_OP_BNE   0x0Du
#define H64_OP_HALT  0x3Fu

#ifdef __cplusplus
}
#endif

#endif /* HYBRID64_HAL_H */
