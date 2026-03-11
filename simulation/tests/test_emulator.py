"""
test_emulator.py — Unit tests for the Hybrid-64 Python emulator

Run with:  python -m pytest simulation/tests/ -v
"""
import sys
import os
import pytest

# Make simulation/ importable
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from emulator import Alu64, AluOp, Cpu64, Op, _u64, _i64, UINT64_MAX


# ══════════════════════════════════════════════════════════════════════════════
# ALU tests
# ══════════════════════════════════════════════════════════════════════════════
class TestAlu:
    def setup_method(self):
        self.alu = Alu64()

    def op(self, o, a, b=0):
        return self.alu.execute(o, a, b)

    # ── ADD ──────────────────────────────────────────────────────────────────
    def test_add_basic(self):
        assert self.op(AluOp.ADD, 100, 200) == 300

    def test_add_zero_flag(self):
        assert self.op(AluOp.ADD, 0, 0) == 0
        assert self.alu.flags.zero

    def test_add_no_zero_flag(self):
        self.op(AluOp.ADD, 1, 1)
        assert not self.alu.flags.zero

    def test_add_carry(self):
        self.op(AluOp.ADD, UINT64_MAX, 1)
        assert self.alu.flags.carry

    def test_add_wrap(self):
        assert self.op(AluOp.ADD, UINT64_MAX, 1) == 0

    def test_add_overflow_positive(self):
        """Adding two large positives that flip sign bit = overflow."""
        self.op(AluOp.ADD, (1 << 62), (1 << 62))
        assert self.alu.flags.overflow

    # ── SUB ──────────────────────────────────────────────────────────────────
    def test_sub_basic(self):
        assert self.op(AluOp.SUB, 500, 123) == 377

    def test_sub_zero(self):
        assert self.op(AluOp.SUB, 42, 42) == 0
        assert self.alu.flags.zero

    def test_sub_wrap(self):
        assert self.op(AluOp.SUB, 0, 1) == UINT64_MAX

    # ── Bitwise ──────────────────────────────────────────────────────────────
    def test_and(self):
        assert self.op(AluOp.AND, 0xFF00FF00, 0x0F0F0F0F) == 0x0F000F00

    def test_or(self):
        assert self.op(AluOp.OR,  0xAA000000, 0x00550000) == 0xAA550000

    def test_xor_same(self):
        v = 0xDEADBEEFDEADBEEF
        assert self.op(AluOp.XOR, v, v) == 0
        assert self.alu.flags.zero

    def test_not(self):
        assert self.op(AluOp.NOT, 0) == UINT64_MAX

    def test_nand(self):
        assert self.op(AluOp.NAND, UINT64_MAX, UINT64_MAX) == 0

    def test_nor(self):
        assert self.op(AluOp.NOR, 0, 0) == UINT64_MAX

    def test_xnor(self):
        v = 0xABCD1234ABCD1234
        assert self.op(AluOp.XNOR, v, v) == UINT64_MAX

    # ── Shifts ───────────────────────────────────────────────────────────────
    def test_shl(self):
        assert self.op(AluOp.SHL, 1, 4) == 16

    def test_shr(self):
        assert self.op(AluOp.SHR, 0x100, 4) == 0x10

    def test_sar_positive(self):
        assert self.op(AluOp.SAR, 0x100, 4) == 0x10

    def test_sar_negative(self):
        neg = _u64(-256)
        result = self.op(AluOp.SAR, neg, 4)
        assert _i64(result) == -16

    def test_shl_overflow(self):
        """Shift >= 64 should give 0."""
        assert self.op(AluOp.SHL, 1, 64) == 0

    # ── Multiply / divide ─────────────────────────────────────────────────
    def test_mul(self):
        assert self.op(AluOp.MUL, 7, 6) == 42

    def test_div(self):
        assert self.op(AluOp.DIV, 100, 7) == 14

    def test_div_by_zero(self):
        assert self.op(AluOp.DIV, 100, 0) == UINT64_MAX

    def test_mod(self):
        assert self.op(AluOp.MOD, 100, 7) == 2

    def test_mod_by_zero(self):
        assert self.op(AluOp.MOD, 100, 0) == 0

    # ── Compare ──────────────────────────────────────────────────────────────
    def test_cmp_equal(self):
        assert self.op(AluOp.CMP, 42, 42) == 0

    def test_cmp_greater(self):
        assert self.op(AluOp.CMP, 50, 42) == 1

    def test_cmp_less(self):
        assert self.op(AluOp.CMP, 42, 50) == UINT64_MAX


# ══════════════════════════════════════════════════════════════════════════════
# CPU / instruction tests
# ══════════════════════════════════════════════════════════════════════════════
class TestCpu:
    def _cpu_with(self, instrs):
        cpu = Cpu64()
        cpu.mem.load_program(instrs)
        return cpu

    # ── ADDI ─────────────────────────────────────────────────────────────────
    def test_addi(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 42),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[1] == 42

    def test_addi_negative_imm(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 100),
            Cpu64.mk_i(Op.ADDI, 1, 1, -10 & 0x7FF),  # r1 = r1 + (-10)
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[1] == 90

    # ── ADD / SUB ─────────────────────────────────────────────────────────────
    def test_add(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 10),
            Cpu64.mk_i(Op.ADDI, 0, 2, 30),
            Cpu64.mk_r(Op.ADD, 1, 2, 3),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 40

    def test_sub(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 30),
            Cpu64.mk_i(Op.ADDI, 0, 2, 10),
            Cpu64.mk_r(Op.SUB, 1, 2, 3),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 20

    # ── AND / OR / XOR ────────────────────────────────────────────────────────
    def test_and(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 0xFF),
            Cpu64.mk_i(Op.ADDI, 0, 2, 0x0F),
            Cpu64.mk_r(Op.AND, 1, 2, 3),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 0x0F

    def test_or(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 0xF0),
            Cpu64.mk_i(Op.ADDI, 0, 2, 0x0F),
            Cpu64.mk_r(Op.OR, 1, 2, 3),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 0xFF

    def test_xor(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 0xFF),
            Cpu64.mk_i(Op.ADDI, 0, 2, 0xFF),
            Cpu64.mk_r(Op.XOR, 1, 2, 3),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 0

    # ── SHL / SHR ─────────────────────────────────────────────────────────────
    def test_shl(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 1),
            Cpu64.mk_i(Op.ADDI, 0, 2, 4),
            Cpu64.mk_r(Op.SHL, 1, 2, 3),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 16

    def test_shr(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 0x40),
            Cpu64.mk_i(Op.ADDI, 0, 2, 2),
            Cpu64.mk_r(Op.SHR, 1, 2, 3),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 0x10

    # ── LD / ST ───────────────────────────────────────────────────────────────
    def test_store_load(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 0x100),  # r1 = 0x100 (base addr)
            Cpu64.mk_i(Op.ADDI, 0, 2, 99),      # r2 = 99 (value to store)
            Cpu64.mk_rs(Op.ST, 1, 2, 0),         # mem[r1+0] = r2
            Cpu64.mk_i(Op.LD, 1, 3, 0),          # r3 = mem[r1+0]
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 99

    def test_store_load_offset(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 0x200),  # r1 = base
            Cpu64.mk_i(Op.ADDI, 0, 2, 7),       # r2 = 7
            Cpu64.mk_rs(Op.ST, 1, 2, 8),         # mem[r1+8] = r2
            Cpu64.mk_i(Op.LD, 1, 4, 8),          # r4 = mem[r1+8]
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[4] == 7

    # ── Branches ─────────────────────────────────────────────────────────────
    def test_beq_taken(self):
        """BEQ taken: r1 == r2 → jump forward, skipping the ADDI."""
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 5),         # PC=0:  r1=5
            Cpu64.mk_i(Op.ADDI, 0, 2, 5),          # PC=4:  r2=5
            Cpu64.mk_rs(Op.BEQ, 1, 2, 2),          # PC=8:  if r1==r2 jump to PC=16
            Cpu64.mk_i(Op.ADDI, 0, 3, 99),         # PC=12: skipped
            Cpu64.INSTR_HALT,                       # PC=16
        ])
        cpu.run()
        assert cpu.state.regs[3] == 0

    def test_beq_not_taken(self):
        """BEQ not taken: r1 != r2 → fall through and execute ADDI."""
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 5),
            Cpu64.mk_i(Op.ADDI, 0, 2, 6),          # r2 != r1
            Cpu64.mk_rs(Op.BEQ, 1, 2, 2),          # NOT taken; fall through
            Cpu64.mk_i(Op.ADDI, 0, 3, 42),         # r3=42 (executed)
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[3] == 42

    def test_bne_loop(self):
        """Loop: r1 = sum(1..10) using BNE."""
        cpu = Cpu64()
        prog = [
            Cpu64.mk_i(Op.ADDI, 0, 1,  0),            # r1=0 (acc)
            Cpu64.mk_i(Op.ADDI, 0, 2,  0),            # r2=0 (counter)
            Cpu64.mk_i(Op.ADDI, 0, 3, 10),            # r3=10 (limit)
            Cpu64.mk_i(Op.ADDI, 0, 4,  1),            # r4=1 (step)
            # loop at PC=16 (instr index 4)
            Cpu64.mk_r(Op.ADD, 2, 4, 2),               # r2 += 1
            Cpu64.mk_r(Op.ADD, 1, 2, 1),               # r1 += r2
            Cpu64.mk_rs(Op.BNE, 2, 3, -2 & 0x7FF),    # if r2!=r3 goto -2
            Cpu64.INSTR_HALT,
        ]
        cpu.mem.load_program(prog)
        cpu.run(max_cycles=10000)
        assert cpu.state.regs[1] == 55  # 1+2+…+10

    # ── Register 0 is read-only ────────────────────────────────────────────
    def test_r0_readonly(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 0, 99),   # try to write r0
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[0] == 0

    # ── HALT ─────────────────────────────────────────────────────────────────
    def test_halt(self):
        cpu = self._cpu_with([Cpu64.INSTR_HALT])
        cpu.run()
        assert cpu.state.halted

    def test_halt_stops_execution(self):
        cpu = self._cpu_with([
            Cpu64.INSTR_HALT,
            Cpu64.mk_i(Op.ADDI, 0, 1, 99),  # must not execute
        ])
        cpu.run()
        assert cpu.state.regs[1] == 0

    # ── NOP ──────────────────────────────────────────────────────────────────
    def test_nop(self):
        cpu = self._cpu_with([
            Cpu64.mk_r(Op.NOP, 0, 0, 0),
            Cpu64.mk_i(Op.ADDI, 0, 1, 7),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.regs[1] == 7

    # ── Cycle counter ─────────────────────────────────────────────────────────
    def test_cycle_count(self):
        cpu = self._cpu_with([
            Cpu64.mk_i(Op.ADDI, 0, 1, 1),
            Cpu64.mk_i(Op.ADDI, 0, 2, 2),
            Cpu64.INSTR_HALT,
        ])
        cpu.run()
        assert cpu.state.cycles == 3  # 2 ADDI + 1 HALT


# ══════════════════════════════════════════════════════════════════════════════
# Memory tests
# ══════════════════════════════════════════════════════════════════════════════
class TestMemory:
    def test_read_write_8(self):
        from emulator import Memory64
        m = Memory64(256)
        m.write8(10, 0xAB)
        assert m.read8(10) == 0xAB

    def test_read_write_32(self):
        from emulator import Memory64
        m = Memory64(256)
        m.write32(0, 0xDEADBEEF)
        assert m.read32(0) == 0xDEADBEEF

    def test_read_write_64(self):
        from emulator import Memory64
        m = Memory64(256)
        m.write64(0, 0xCAFEBABEDEADBEEF)
        assert m.read64(0) == 0xCAFEBABEDEADBEEF

    def test_little_endian_byte_order(self):
        from emulator import Memory64
        m = Memory64(256)
        m.write32(0, 0x01020304)
        assert m.read8(0) == 0x04  # LSB at lowest address
        assert m.read8(1) == 0x03
        assert m.read8(2) == 0x02
        assert m.read8(3) == 0x01
