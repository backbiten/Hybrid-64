# Hybrid-64

**A hybrid software/hardware architecture for a 64-bit computing system.**

Hybrid-64 combines synthesisable hardware description (Verilog RTL) with a
matching software emulation layer (C HAL + Python emulator), enabling
development without FPGA hardware, cross-verification, and deployment to real
devices.

## Quick Start

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
```

## Repository Structure

| Path | Contents |
|---|---|
| `hardware/rtl/` | Verilog RTL: ALU, register file, CPU core, memory |
| `hardware/sim/` | HDL testbenches |
| `hardware/constraints/` | FPGA constraints (Xilinx XDC) |
| `software/hal/` | C Hardware Abstraction Layer |
| `software/drivers/` | Higher-level CPU driver |
| `software/apps/` | Reference application |
| `simulation/` | Python emulator + unit tests |
| `docs/` | Architecture documentation |

See **[docs/architecture.md](docs/architecture.md)** for the full architecture
guide, ISA reference, and build instructions.
