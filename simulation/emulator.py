"""
emulator.py — Hybrid-64 Python software emulator

Mirrors the RTL behaviour of the Hybrid-64 CPU core in pure Python,
enabling fast simulation, test automation, and cross-validation against
the Verilog simulation and the C HAL.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Optional
import struct

# ── Constants ──────────────────────────────────────────────────────────────
NUM_REGS    = 32
WORD_SIZE   = 8        # bytes per 64-bit word
INSTR_SIZE  = 4        # bytes per instruction
UINT64_MAX  = (1 << 64) - 1
INT64_MIN   = -(1 << 63)
INT64_MAX   = (1 << 63) - 1


def _u64(v: int) -> int:
    """Truncate to unsigned 64-bit."""
    return v & UINT64_MAX


def _i64(v: int) -> int:
    """Reinterpret unsigned 64-bit as signed 64-bit."""
    v = _u64(v)
    return v if v <= INT64_MAX else v - (1 << 64)


def _sext11(v: int) -> int:
    """Sign-extend an 11-bit value to 64-bit."""
    v &= 0x7FF
    return v | -(v & 0x400)


def _sext21(v: int) -> int:
    """Sign-extend a 21-bit value to 64-bit."""
    v &= 0x1FFFFF
    return v | -(v & 0x100000)


# ── ALU operation codes ────────────────────────────────────────────────────
class AluOp(IntEnum):
    ADD  = 0x0
    SUB  = 0x1
    AND  = 0x2
    OR   = 0x3
    XOR  = 0x4
    NOT  = 0x5
    SHL  = 0x6
    SHR  = 0x7
    SAR  = 0x8
    MUL  = 0x9
    DIV  = 0xA
    MOD  = 0xB
    CMP  = 0xC
    NAND = 0xD
    NOR  = 0xE
    XNOR = 0xF


# ── Opcode constants ───────────────────────────────────────────────────────
class Op(IntEnum):
    NOP  = 0x00
    ADD  = 0x01
    SUB  = 0x02
    AND  = 0x03
    OR   = 0x04
    XOR  = 0x05
    SHL  = 0x06
    SHR  = 0x07
    LD   = 0x08
    ST   = 0x09
    ADDI = 0x0A
    JMP  = 0x0B
    BEQ  = 0x0C
    BNE  = 0x0D
    HALT = 0x3F


# ── CPU flags ──────────────────────────────────────────────────────────────
@dataclass
class Flags:
    zero:     bool = False
    carry:    bool = False
    overflow: bool = False


# ── CPU state ──────────────────────────────────────────────────────────────
@dataclass
class CpuState:
    regs:   list[int]  = field(default_factory=lambda: [0] * NUM_REGS)
    pc:     int        = 0
    flags:  Flags      = field(default_factory=Flags)
    halted: bool       = False
    cycles: int        = 0


# ── ALU ────────────────────────────────────────────────────────────────────
class Alu64:
    """64-bit ALU — mirrors alu64.v"""

    def __init__(self) -> None:
        self.flags = Flags()

    def execute(self, op: AluOp, a: int, b: int) -> int:
        a = _u64(a)
        b = _u64(b)
        carry = overflow = False
        result = 0

        if op == AluOp.ADD:
            wide   = a + b
            result = _u64(wide)
            carry  = wide > UINT64_MAX
            overflow = (not (a >> 63) and not (b >> 63) and bool(result >> 63)) or \
                       (bool(a >> 63) and bool(b >> 63) and not (result >> 63))
        elif op == AluOp.SUB:
            wide   = a - b
            result = _u64(wide)
            carry  = wide < 0
            overflow = (not (a >> 63) and bool(b >> 63) and bool(result >> 63)) or \
                       (bool(a >> 63) and not (b >> 63) and not (result >> 63))
        elif op == AluOp.AND:  result = a & b
        elif op == AluOp.OR:   result = a | b
        elif op == AluOp.XOR:  result = a ^ b
        elif op == AluOp.NOT:  result = _u64(~a)
        elif op == AluOp.SHL:  result = _u64(a << (b & 0x3F)) if b < 64 else 0
        elif op == AluOp.SHR:  result = a >> (b & 0x3F) if b < 64 else 0
        elif op == AluOp.SAR:
            result = _u64(_i64(a) >> (b & 0x3F)) if b < 64 else 0
        elif op == AluOp.MUL:  result = _u64(a * b)
        elif op == AluOp.DIV:  result = a // b if b != 0 else UINT64_MAX
        elif op == AluOp.MOD:  result = a % b  if b != 0 else 0
        elif op == AluOp.CMP:
            result = 0 if a == b else (1 if a > b else UINT64_MAX)
        elif op == AluOp.NAND: result = _u64(~(a & b))
        elif op == AluOp.NOR:  result = _u64(~(a | b))
        elif op == AluOp.XNOR: result = _u64(~(a ^ b))

        self.flags.zero     = (result == 0)
        self.flags.carry    = carry
        self.flags.overflow = overflow
        return _u64(result)


# ── Memory ──────────────────────────────────────────────────────────────────
class Memory64:
    """Byte-addressable memory, little-endian, mirrors memory64.v"""

    def __init__(self, size: int = 64 * 1024) -> None:
        self._mem = bytearray(size)
        self.size = size

    def read8(self, addr: int) -> int:
        return self._mem[addr & (self.size - 1)]

    def write8(self, addr: int, val: int) -> None:
        self._mem[addr & (self.size - 1)] = val & 0xFF

    def read32(self, addr: int) -> int:
        b = self._mem[addr:addr + 4]
        return struct.unpack_from("<I", b)[0]

    def write32(self, addr: int, val: int) -> None:
        struct.pack_into("<I", self._mem, addr, val & 0xFFFFFFFF)

    def read64(self, addr: int) -> int:
        b = self._mem[addr:addr + 8]
        return struct.unpack_from("<Q", b)[0]

    def write64(self, addr: int, val: int) -> None:
        struct.pack_into("<Q", self._mem, addr, _u64(val))

    def load_program(self, instrs: list[int], base: int = 0) -> None:
        """Write a list of 32-bit instruction words starting at *base*."""
        for i, instr in enumerate(instrs):
            self.write32(base + i * INSTR_SIZE, instr)

    def hexdump(self, addr: int, length: int = 64) -> str:
        lines = []
        for off in range(0, length, 16):
            chunk = self._mem[addr + off:addr + off + 16]
            hex_part  = " ".join(f"{b:02x}" for b in chunk)
            ascii_part = "".join(chr(b) if 0x20 <= b < 0x7F else "." for b in chunk)
            lines.append(f"  {addr+off:016x}: {hex_part:<48}  |{ascii_part}|")
        return "\n".join(lines)


# ── CPU ────────────────────────────────────────────────────────────────────
class Cpu64:
    """
    Single-cycle Hybrid-64 CPU emulator.

    Decodes and executes one instruction per call to :meth:`step`.
    Instruction encoding matches cpu_core.v.
    """

    def __init__(self, mem_size: int = 64 * 1024) -> None:
        self.mem  = Memory64(mem_size)
        self.alu  = Alu64()
        self.state = CpuState()

    # ── Helpers ────────────────────────────────────────────────────────────
    def _r(self, idx: int) -> int:
        return 0 if idx == 0 else self.state.regs[idx]

    def _w(self, idx: int, val: int) -> None:
        if idx != 0:
            self.state.regs[idx] = _u64(val)

    # ── Single step ────────────────────────────────────────────────────────
    def step(self) -> bool:
        """Execute one instruction.  Returns True if the CPU has halted."""
        if self.state.halted:
            return True

        pc    = self.state.pc
        instr = self.mem.read32(pc)

        op  = (instr >> 26) & 0x3F
        rs1 = (instr >> 21) & 0x1F
        rs2 = (instr >> 16) & 0x1F
        rd  = (instr >> 11) & 0x1F
        imm = _sext11(instr & 0x7FF)

        a    = self._r(rs1)
        b    = self._r(rs2)
        next_pc = _u64(pc + INSTR_SIZE)

        try:
            opcode = Op(op)
        except ValueError:
            opcode = Op.NOP

        if opcode == Op.NOP:
            pass
        elif opcode == Op.ADD:
            self._w(rd, self.alu.execute(AluOp.ADD, a, b))
        elif opcode == Op.SUB:
            self._w(rd, self.alu.execute(AluOp.SUB, a, b))
        elif opcode == Op.AND:
            self._w(rd, self.alu.execute(AluOp.AND, a, b))
        elif opcode == Op.OR:
            self._w(rd, self.alu.execute(AluOp.OR,  a, b))
        elif opcode == Op.XOR:
            self._w(rd, self.alu.execute(AluOp.XOR, a, b))
        elif opcode == Op.SHL:
            self._w(rd, self.alu.execute(AluOp.SHL, a, b))
        elif opcode == Op.SHR:
            self._w(rd, self.alu.execute(AluOp.SHR, a, b))
        elif opcode == Op.ADDI:
            self._w(rd, self.alu.execute(AluOp.ADD, a, _u64(imm)))
        elif opcode == Op.LD:
            ea = _u64(a + imm)
            self._w(rd, self.mem.read64(ea))
        elif opcode == Op.ST:
            ea = _u64(a + imm)
            self.mem.write64(ea, b)
        elif opcode == Op.JMP:
            off = _sext21(instr & 0x1FFFFF)
            next_pc = _u64(pc + off)
        elif opcode == Op.BEQ:
            self.alu.execute(AluOp.SUB, a, b)
            if self.alu.flags.zero:
                next_pc = _u64(pc + imm * INSTR_SIZE)
        elif opcode == Op.BNE:
            self.alu.execute(AluOp.SUB, a, b)
            if not self.alu.flags.zero:
                next_pc = _u64(pc + imm * INSTR_SIZE)
        elif opcode == Op.HALT:
            self.state.halted = True

        self.state.flags  = Flags(
            zero     = self.alu.flags.zero,
            carry    = self.alu.flags.carry,
            overflow = self.alu.flags.overflow,
        )

        if not self.state.halted:
            self.state.pc = next_pc
        self.state.cycles += 1
        return self.state.halted

    def run(self, max_cycles: int = 0) -> CpuState:
        """Run until HALT or max_cycles (0 = unlimited)."""
        count = 0
        while not self.state.halted:
            self.step()
            count += 1
            if max_cycles and count >= max_cycles:
                break
        return self.state

    def reset(self) -> None:
        """Reset CPU state (does not clear memory)."""
        self.state = CpuState()

    # ── Instruction assembly helpers ────────────────────────────────────────
    @staticmethod
    def mk_r(op: int, rs1: int, rs2: int, rd: int) -> int:
        """R-type: op rs1, rs2 → rd"""
        return ((op & 0x3F) << 26 | (rs1 & 0x1F) << 21 |
                (rs2 & 0x1F) << 16 | (rd  & 0x1F) << 11)

    @staticmethod
    def mk_i(op: int, rs1: int, rd: int, imm: int) -> int:
        """I-type (dest = rd): op rs1, imm → rd  (ADDI, LD)"""
        return ((op & 0x3F) << 26 | (rs1 & 0x1F) << 21 |
                (rd  & 0x1F) << 11 | (imm & 0x7FF))

    @staticmethod
    def mk_rs(op: int, rs1: int, rs2: int, imm: int) -> int:
        """RS-type (two source regs + imm): used for ST, BEQ, BNE.
        Encodes rs2 at [20:16] and imm at [10:0]."""
        return ((op & 0x3F) << 26 | (rs1 & 0x1F) << 21 |
                (rs2 & 0x1F) << 16 | (imm & 0x7FF))

    INSTR_HALT: int = 0xFC000000

    # ── Debug ───────────────────────────────────────────────────────────────
    def dump_regs(self) -> str:
        s = self.state
        lines = [
            f"PC=0x{s.pc:016x}  cycles={s.cycles}"
            f"  {'HALTED ' if s.halted else ''}"
            f"{'Z' if s.flags.zero else '-'}"
            f"{'C' if s.flags.carry else '-'}"
            f"{'V' if s.flags.overflow else '-'}"
        ]
        for i in range(0, NUM_REGS, 4):
            lines.append(
                f"  r{i:<2}=0x{s.regs[i]:016x}  r{i+1:<2}=0x{s.regs[i+1]:016x}"
                f"  r{i+2:<2}=0x{s.regs[i+2]:016x}  r{i+3:<2}=0x{s.regs[i+3]:016x}"
            )
        return "\n".join(lines)
