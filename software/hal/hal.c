/**
 * hal.c — Hybrid-64 Hardware Abstraction Layer implementation
 *
 * Software-emulator backend.  FPGA/SIM backends would add their own
 * hal_open() variants and function-pointer dispatch tables.
 */
#include "hal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* ── Internal device structure ────────────────────────────────────────────── */
struct hal_device {
    hal_backend_t backend;
    size_t        mem_size;
    uint8_t      *mem;

    /* CPU architectural state */
    uint64_t regs[H64_NUM_REGS]; /* r0 is always 0; writes to r0 are ignored */
    uint64_t pc;
    bool     halted;
    bool     flag_zero;
    bool     flag_carry;
    bool     flag_overflow;
    uint64_t cycles;
};

/* ── Error strings ────────────────────────────────────────────────────────── */
const char *hal_strerror(hal_status_t err)
{
    switch (err) {
    case HAL_OK:              return "success";
    case HAL_ERR_NULL:        return "null pointer";
    case HAL_ERR_RANGE:       return "address or index out of range";
    case HAL_ERR_ALIGN:       return "misaligned access";
    case HAL_ERR_HALTED:      return "CPU is halted";
    case HAL_ERR_RUNNING:     return "CPU is running";
    case HAL_ERR_UNSUPPORTED: return "operation not supported";
    case HAL_ERR_IO:          return "I/O error";
    default:                  return "unknown error";
    }
}

/* ── Lifecycle ────────────────────────────────────────────────────────────── */
hal_status_t hal_open(hal_backend_t backend, size_t mem_size,
                      hal_device_t **out)
{
    if (!out)
        return HAL_ERR_NULL;
    if (backend != HAL_BACKEND_SOFTWARE)
        return HAL_ERR_UNSUPPORTED;  /* Other backends not implemented yet */

    if (mem_size == 0)
        mem_size = H64_MEM_SIZE;

    /* Require power-of-2 size, at least 256 bytes */
    if (mem_size < 256 || (mem_size & (mem_size - 1)) != 0)
        return HAL_ERR_RANGE;

    hal_device_t *dev = calloc(1, sizeof(*dev));
    if (!dev)
        return HAL_ERR_IO;

    dev->mem = calloc(1, mem_size);
    if (!dev->mem) {
        free(dev);
        return HAL_ERR_IO;
    }

    dev->backend  = backend;
    dev->mem_size = mem_size;
    *out = dev;
    return HAL_OK;
}

void hal_close(hal_device_t *dev)
{
    if (!dev)
        return;
    free(dev->mem);
    free(dev);
}

/* ── Reset ────────────────────────────────────────────────────────────────── */
hal_status_t hal_reset(hal_device_t *dev)
{
    if (!dev)
        return HAL_ERR_NULL;
    memset(dev->regs, 0, sizeof(dev->regs));
    dev->pc            = 0;
    dev->halted        = false;
    dev->flag_zero     = false;
    dev->flag_carry    = false;
    dev->flag_overflow = false;
    dev->cycles        = 0;
    return HAL_OK;
}

/* ── Internal ALU ─────────────────────────────────────────────────────────── */
static uint64_t emu_alu(hal_device_t *dev,
                        hal_alu_op_t op,
                        uint64_t a, uint64_t b)
{
    uint64_t result = 0;
    bool carry = false, overflow = false;

    switch (op) {
    case HAL_ALU_ADD: {
        result   = a + b;                 /* uint64_t wraps naturally */
        carry    = (result < a);
        overflow = ((~a >> 63 & ~b >> 63 & result >> 63) |
                    ( a >> 63 &  b >> 63 & ~result >> 63)) & 1u;
        break;
    }
    case HAL_ALU_SUB: {
        result   = a - b;
        carry    = (a < b);
        overflow = ((~a >> 63 &  b >> 63 & result >> 63) |
                    ( a >> 63 & ~b >> 63 & ~result >> 63)) & 1u;
        break;
    }
    case HAL_ALU_AND:  result = a & b;  break;
    case HAL_ALU_OR:   result = a | b;  break;
    case HAL_ALU_XOR:  result = a ^ b;  break;
    case HAL_ALU_NOT:  result = ~a;     break;
    case HAL_ALU_SHL:  result = (b < 64) ? a << b : 0;         break;
    case HAL_ALU_SHR:  result = (b < 64) ? a >> b : 0;         break;
    case HAL_ALU_SAR:  result = (b < 64) ? (uint64_t)((int64_t)a >> b) : 0; break;
    case HAL_ALU_MUL:  result = a * b;  break;
    case HAL_ALU_DIV:  result = (b != 0) ? a / b : UINT64_MAX; break;
    case HAL_ALU_MOD:  result = (b != 0) ? a % b : 0;          break;
    case HAL_ALU_CMP:
        result = (a == b) ? 0 : (a > b) ? 1 : UINT64_MAX;
        break;
    case HAL_ALU_NAND: result = ~(a & b); break;
    case HAL_ALU_NOR:  result = ~(a | b); break;
    case HAL_ALU_XNOR: result = ~(a ^ b); break;
    }

    dev->flag_zero     = (result == 0);
    dev->flag_carry    = carry;
    dev->flag_overflow = overflow;
    return result;
}

/* ── Memory helpers ───────────────────────────────────────────────────────── */
static inline uint32_t mem_read32(const hal_device_t *dev, uint64_t addr)
{
    if (addr + 4 > dev->mem_size)
        return 0;
    uint32_t v;
    memcpy(&v, dev->mem + addr, 4);
    return v;
}

static inline uint64_t mem_read64(const hal_device_t *dev, uint64_t addr)
{
    if (addr + 8 > dev->mem_size)
        return 0;
    uint64_t v;
    memcpy(&v, dev->mem + addr, 8);
    return v;
}

static inline void mem_write64(hal_device_t *dev, uint64_t addr, uint64_t val)
{
    if (addr + 8 > dev->mem_size)
        return;
    memcpy(dev->mem + addr, &val, 8);
}

/* ── Instruction decode & execute (single step) ───────────────────────────── */
hal_status_t hal_step(hal_device_t *dev)
{
    if (!dev)
        return HAL_ERR_NULL;
    if (dev->halted)
        return HAL_ERR_HALTED;

    uint32_t instr = mem_read32(dev, dev->pc);

    uint8_t  op   = (instr >> 26) & 0x3F;
    uint8_t  rs1  = (instr >> 21) & 0x1F;
    uint8_t  rs2  = (instr >> 16) & 0x1F;
    uint8_t  rd   = (instr >> 11) & 0x1F;
    int16_t  imm  = (int16_t)((instr & 0x7FF) | ((instr & 0x400) ? 0xF800u : 0));

    uint64_t a    = dev->regs[rs1];
    uint64_t b    = dev->regs[rs2];
    uint64_t imm_u = (uint64_t)(int64_t)imm;

    uint64_t next_pc = dev->pc + H64_INSTR_SIZE;
    uint64_t result  = 0;
    bool     write_rd = false;

    switch (op) {
    case H64_OP_NOP:  break;
    case H64_OP_ADD:  result = emu_alu(dev, HAL_ALU_ADD, a, b); write_rd = true; break;
    case H64_OP_SUB:  result = emu_alu(dev, HAL_ALU_SUB, a, b); write_rd = true; break;
    case H64_OP_AND:  result = emu_alu(dev, HAL_ALU_AND, a, b); write_rd = true; break;
    case H64_OP_OR:   result = emu_alu(dev, HAL_ALU_OR,  a, b); write_rd = true; break;
    case H64_OP_XOR:  result = emu_alu(dev, HAL_ALU_XOR, a, b); write_rd = true; break;
    case H64_OP_SHL:  result = emu_alu(dev, HAL_ALU_SHL, a, b); write_rd = true; break;
    case H64_OP_SHR:  result = emu_alu(dev, HAL_ALU_SHR, a, b); write_rd = true; break;
    case H64_OP_ADDI:
        result = emu_alu(dev, HAL_ALU_ADD, a, imm_u);
        write_rd = true;
        break;
    case H64_OP_LD: {
        uint64_t ea = a + imm_u;
        result   = mem_read64(dev, ea);
        write_rd = true;
        /* Re-compute ALU flags from address calculation */
        emu_alu(dev, HAL_ALU_ADD, a, imm_u);
        break;
    }
    case H64_OP_ST: {
        uint64_t ea = a + imm_u;
        mem_write64(dev, ea, b);
        break;
    }
    case H64_OP_JMP: {
        /* 21-bit signed offset in [20:0] */
        int32_t off = (int32_t)(instr & 0x1FFFFF);
        if (off & 0x100000) off |= (int32_t)0xFFE00000;
        next_pc = dev->pc + (int64_t)off;
        break;
    }
    case H64_OP_BEQ:
        emu_alu(dev, HAL_ALU_SUB, a, b);
        if (dev->flag_zero)
            next_pc = dev->pc + (int64_t)(int16_t)(imm * H64_INSTR_SIZE);
        break;
    case H64_OP_BNE:
        emu_alu(dev, HAL_ALU_SUB, a, b);
        if (!dev->flag_zero)
            next_pc = dev->pc + (int64_t)(int16_t)(imm * H64_INSTR_SIZE);
        break;
    case H64_OP_HALT:
        dev->halted = true;
        break;
    default:
        break;
    }

    if (write_rd && rd != 0)
        dev->regs[rd] = result;
    dev->regs[0] = 0;  /* r0 always reads as 0 */

    if (!dev->halted)
        dev->pc = next_pc;

    dev->cycles++;
    return HAL_OK;
}

/* ── Run ──────────────────────────────────────────────────────────────────── */
hal_status_t hal_run(hal_device_t *dev, uint64_t max_cycles)
{
    if (!dev)
        return HAL_ERR_NULL;
    if (dev->halted)
        return HAL_ERR_HALTED;

    uint64_t count = 0;
    while (!dev->halted) {
        hal_status_t st = hal_step(dev);
        if (st != HAL_OK && st != HAL_ERR_HALTED)
            return st;
        if (++count == max_cycles && max_cycles != 0)
            break;
    }
    return HAL_OK;
}

/* ── Register access ──────────────────────────────────────────────────────── */
hal_status_t hal_reg_read(const hal_device_t *dev, unsigned reg,
                          uint64_t *value)
{
    if (!dev || !value)  return HAL_ERR_NULL;
    if (reg >= H64_NUM_REGS) return HAL_ERR_RANGE;
    *value = dev->regs[reg];
    return HAL_OK;
}

hal_status_t hal_reg_write(hal_device_t *dev, unsigned reg, uint64_t value)
{
    if (!dev)            return HAL_ERR_NULL;
    if (reg >= H64_NUM_REGS) return HAL_ERR_RANGE;
    if (reg == 0)        return HAL_OK;  /* r0 is read-only */
    dev->regs[reg] = value;
    return HAL_OK;
}

hal_status_t hal_reg_read_all(const hal_device_t *dev,
                              uint64_t buf[H64_NUM_REGS])
{
    if (!dev || !buf) return HAL_ERR_NULL;
    memcpy(buf, dev->regs, sizeof(dev->regs));
    return HAL_OK;
}

/* ── Memory access ────────────────────────────────────────────────────────── */
hal_status_t hal_mem_read(const hal_device_t *dev, uint64_t addr,
                          void *buf, size_t len)
{
    if (!dev || !buf)           return HAL_ERR_NULL;
    if (addr + len > dev->mem_size) return HAL_ERR_RANGE;
    memcpy(buf, dev->mem + addr, len);
    return HAL_OK;
}

hal_status_t hal_mem_write(hal_device_t *dev, uint64_t addr,
                           const void *buf, size_t len)
{
    if (!dev || !buf)           return HAL_ERR_NULL;
    if (addr + len > dev->mem_size) return HAL_ERR_RANGE;
    memcpy(dev->mem + addr, buf, len);
    return HAL_OK;
}

hal_status_t hal_mem_write_instr(hal_device_t *dev, uint64_t addr,
                                 uint32_t instr)
{
    return hal_mem_write(dev, addr, &instr, sizeof(instr));
}

hal_status_t hal_mem_read64(const hal_device_t *dev, uint64_t addr,
                             uint64_t *value)
{
    if (!dev || !value)         return HAL_ERR_NULL;
    if (addr % H64_WORD_SIZE)   return HAL_ERR_ALIGN;
    if (addr + 8 > dev->mem_size) return HAL_ERR_RANGE;
    *value = mem_read64(dev, addr);
    return HAL_OK;
}

hal_status_t hal_mem_write64(hal_device_t *dev, uint64_t addr, uint64_t value)
{
    if (!dev)                   return HAL_ERR_NULL;
    if (addr % H64_WORD_SIZE)   return HAL_ERR_ALIGN;
    if (addr + 8 > dev->mem_size) return HAL_ERR_RANGE;
    mem_write64(dev, addr, value);
    return HAL_OK;
}

/* ── Status ───────────────────────────────────────────────────────────────── */
hal_status_t hal_get_status(const hal_device_t *dev, hal_cpu_status_t *status)
{
    if (!dev || !status) return HAL_ERR_NULL;
    status->halted   = dev->halted;
    status->zero     = dev->flag_zero;
    status->carry    = dev->flag_carry;
    status->overflow = dev->flag_overflow;
    status->pc       = dev->pc;
    status->cycles   = dev->cycles;
    return HAL_OK;
}
