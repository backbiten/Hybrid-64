# Hybrid-64 Architecture Documentation

## Overview

Hybrid-64 is a hybrid software/hardware architecture for a 64-bit computing
system.  It combines synthesisable hardware description (Verilog RTL) with a
matching software emulation layer (C HAL + Python emulator), enabling:

- **Development** without FPGA hardware (software emulator)
- **Verification** by cross-running identical programs on RTL simulation and
  the software emulator and comparing results
- **Deployment** on real FPGA hardware via the HAL FPGA backend

---

## Repository Layout

```
Hybrid-64/
├── hardware/
│   ├── rtl/                 Synthesisable Verilog RTL
│   │   ├── alu64.v          64-bit ALU (16 operations)
│   │   ├── regfile64.v      32 × 64-bit register file
│   │   ├── cpu_core.v       5-stage pipeline CPU core
│   │   └── memory64.v       Byte-addressable unified memory
│   ├── sim/                 HDL testbenches
│   │   ├── tb_alu64.v       Directed ALU tests
│   │   └── tb_cpu.v         CPU integration testbench
│   └── constraints/
│       └── hybrid64.xdc     Xilinx XDC FPGA constraints (Artix-7)
├── software/
│   ├── hal/                 Hardware Abstraction Layer (C11)
│   │   ├── hal.h            Public API
│   │   ├── hal.c            Software-emulator backend
│   │   └── test_hal.c       HAL unit tests
│   ├── drivers/             Higher-level CPU driver
│   │   ├── cpu_driver.h
│   │   └── cpu_driver.c
│   └── apps/
│       └── main.c           Reference application
├── simulation/
│   ├── emulator.py          Python CPU emulator
│   └── tests/
│       └── test_emulator.py pytest unit tests
├── docs/
│   └── architecture.md      ← this file
├── CMakeLists.txt           C build system (CMake ≥ 3.16)
└── Makefile                 Convenience wrapper
```

---

## Instruction Set Architecture

Hybrid-64 uses a fixed 32-bit instruction word with two formats:

### R-type (register operands)
```
 31      26 25   21 20   16 15   11 10        0
 ┌────────┬──────┬──────┬──────┬────────────┐
 │ opcode │  rs1 │  rs2 │  rd  │  (unused)  │
 └────────┴──────┴──────┴──────┴────────────┘
   6 bits   5 b    5 b    5 b     11 bits
```

### I-type (immediate operand)
```
 31      26 25   21 20   16 15   11 10        0
 ┌────────┬──────┬──────┬──────┬────────────┐
 │ opcode │  rs1 │(n/a) │  rd  │  imm[10:0] │
 └────────┴──────┴──────┴──────┴────────────┘
   6 bits   5 b    5 b    5 b     11 bits (sign-extended to 64)
```

### Opcode table

| Opcode | Mnemonic | Format | Operation                        |
|--------|----------|--------|----------------------------------|
| 0x00   | NOP      | R/I    | No operation                     |
| 0x01   | ADD      | R      | rd = rs1 + rs2                   |
| 0x02   | SUB      | R      | rd = rs1 − rs2                   |
| 0x03   | AND      | R      | rd = rs1 & rs2                   |
| 0x04   | OR       | R      | rd = rs1 \| rs2                  |
| 0x05   | XOR      | R      | rd = rs1 ^ rs2                   |
| 0x06   | SHL      | R      | rd = rs1 << rs2[5:0]             |
| 0x07   | SHR      | R      | rd = rs1 >> rs2[5:0] (logical)   |
| 0x08   | LD       | I      | rd = mem64[rs1 + sext(imm)]      |
| 0x09   | ST       | I      | mem64[rs1 + sext(imm)] = rs2     |
| 0x0A   | ADDI     | I      | rd = rs1 + sext(imm)             |
| 0x0B   | JMP      | I      | PC = PC + sext(instr[20:0])      |
| 0x0C   | BEQ      | I      | if rs1==rs2: PC += sext(imm)×4   |
| 0x0D   | BNE      | I      | if rs1≠rs2:  PC += sext(imm)×4   |
| 0x3F   | HALT     | —      | Halt CPU execution               |

**Register conventions:**
- `r0` is hardwired to zero (writes are silently discarded)
- `r1`–`r31` are general-purpose 64-bit registers
- All registers are non-preserved across calls unless explicitly saved

---

## Hardware Layer

### ALU (`hardware/rtl/alu64.v`)
A combinational 64-bit ALU supporting 16 operations (ADD, SUB, AND, OR, XOR,
NOT, SHL, SHR, SAR, MUL, DIV, MOD, CMP, NAND, NOR, XNOR).  Produces `zero`,
`carry`, and `overflow` status flags.

### Register File (`hardware/rtl/regfile64.v`)
32 × 64-bit general-purpose registers with two asynchronous read ports and one
synchronous write port.  Register 0 is permanently zero.

### CPU Core (`hardware/rtl/cpu_core.v`)
Classic 5-stage in-order pipeline:

```
IF → ID → EX → MEM → WB
```

| Stage | Function                                             |
|-------|------------------------------------------------------|
| IF    | Fetch 32-bit instruction from instruction memory     |
| ID    | Decode opcode, read register file                    |
| EX    | Execute ALU operation; compute effective address     |
| MEM   | Read/write data memory                               |
| WB    | Write result back to register file                   |

### Memory (`hardware/rtl/memory64.v`)
Byte-addressable, little-endian, parameterisable depth (default 64 KiB).
Separate instruction (32-bit) and data (64-bit) ports allow simultaneous
instruction fetch and data access (Harvard-like on-chip memory model).

---

## Software Layer

### Hardware Abstraction Layer (`software/hal/`)

The HAL provides a platform-independent C11 API for controlling a Hybrid-64
CPU, whether backed by the software emulator or real FPGA hardware.

```c
// Open a device
hal_device_t *dev;
hal_open(HAL_BACKEND_SOFTWARE, 0, &dev);

// Load and run a program
hal_mem_write_instr(dev, 0, hal_mk_i(H64_OP_ADDI, 0, 1, 42));
hal_mem_write_instr(dev, 4, HAL_INSTR_HALT);
hal_run(dev, 10000);

// Read result
uint64_t v;
hal_reg_read(dev, 1, &v);   // v == 42

hal_close(dev);
```

Key functions:

| Function              | Description                              |
|-----------------------|------------------------------------------|
| `hal_open()`          | Allocate device; choose backend          |
| `hal_close()`         | Release all resources                    |
| `hal_reset()`         | Reset CPU (preserves memory)             |
| `hal_step()`          | Single-cycle step                        |
| `hal_run()`           | Run until HALT or cycle limit            |
| `hal_reg_read/write()`| Read / write individual registers        |
| `hal_mem_read/write()`| Byte-granularity memory access           |
| `hal_mem_write_instr()`| Write a 32-bit instruction word         |
| `hal_get_status()`    | Retrieve PC, flags, halt and cycle count |

### CPU Driver (`software/drivers/`)
Higher-level utilities built on top of the HAL:
- `cpu_load_program()` — reset + bulk instruction load
- `cpu_dump_regs()` — formatted register dump to stdout
- `cpu_dump_mem()` — hexdump of a memory region
- `cpu_run_report()` — run with execution summary

---

## Python Simulation Layer

`simulation/emulator.py` provides a pure-Python functional model that exactly
mirrors the RTL behaviour.  Use it for:
- Rapid program development without building C/RTL
- Golden-reference comparison against the Verilog simulation
- Automated regression testing via pytest

```python
from simulation.emulator import Cpu64, Op

cpu = Cpu64()
prog = [
    Cpu64.mk_i(Op.ADDI, 0, 1, 42),   # r1 = 42
    Cpu64.INSTR_HALT,
]
cpu.mem.load_program(prog)
cpu.run()
assert cpu.state.regs[1] == 42
```

---

## Building and Testing

### Prerequisites

| Tool        | Version | Purpose                        |
|-------------|---------|--------------------------------|
| GCC / Clang | ≥ 9     | C software layer               |
| CMake       | ≥ 3.16  | C build system                 |
| Python      | ≥ 3.9   | Emulator and tests             |
| pytest      | ≥ 7     | Python unit tests              |
| Icarus Verilog | ≥ 11 | Verilog simulation (optional) |

### Quick start

```bash
# Python emulator tests (no C toolchain needed)
python -m pytest simulation/tests/ -v

# Build and test C software layer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build && ctest --output-on-failure

# Verilog simulation (requires Icarus Verilog)
make sim-alu
make sim-cpu

# All-in-one
make all
```

---

## FPGA Synthesis

1. Add all files under `hardware/rtl/` as HDL sources in Vivado / Quartus.
2. Use `hardware/constraints/hybrid64.xdc` as the constraint file (Artix-7).
3. Set the top-level module to `cpu_core` (wrap with a top-level module that
   instantiates `memory64` and optionally a UART debug core).
4. Adjust I/O standards and pin locations for your target board.

---

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Fixed 32-bit instruction width | Simplifies fetch pipeline; matches common RISC ISAs |
| Separate I/D memory ports | Avoids structural hazards in single-cycle memory access |
| 11-bit immediate | Sufficient for small offsets; keeps instruction word compact |
| r0 hardwired to zero | Eliminates need for explicit zero register initialisation |
| Little-endian memory | Matches x86/ARM conventions, easing host-to-device data transfer |
| C11 + Python dual implementation | C for performance/FPGA driver; Python for rapid iteration and testing |

---

## Roadmap

- [ ] Forwarding / hazard detection unit (eliminate NOPs)
- [ ] Interrupt controller
- [ ] MMU / virtual memory
- [ ] FPGA backend for `hal_open()` via PCIe / JTAG
- [ ] Assembler toolchain
- [ ] GCC / LLVM back-end port
