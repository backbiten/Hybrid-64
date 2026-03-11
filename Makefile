# Hybrid-64 Makefile
# Targets: all, sim-alu, sim-cpu, test-python, test-c, clean

# ── Tools ─────────────────────────────────────────────────────────────────────
CC      ?= gcc
IVERILOG?= iverilog
VVP     ?= vvp
PYTHON  ?= python3
PYTEST  ?= pytest
CMAKE   ?= cmake

# ── Flags ─────────────────────────────────────────────────────────────────────
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS :=

# ── Directories ───────────────────────────────────────────────────────────────
BUILD_DIR := build
SIM_DIR   := hardware/sim
RTL_DIR   := hardware/rtl
SW_DIR    := software
SIM_PY    := simulation

.PHONY: all sim-alu sim-cpu test-python test-c cmake-build clean help

all: cmake-build test-python

# ── Verilog simulation ─────────────────────────────────────────────────────────
sim-alu: $(BUILD_DIR)/sim_alu
	@echo "Running ALU testbench..."
	$(VVP) $<

$(BUILD_DIR)/sim_alu: $(SIM_DIR)/tb_alu64.v $(RTL_DIR)/alu64.v
	@mkdir -p $(BUILD_DIR)
	$(IVERILOG) -o $@ $^

sim-cpu: $(BUILD_DIR)/sim_cpu
	@echo "Running CPU testbench..."
	$(VVP) $<

$(BUILD_DIR)/sim_cpu: $(SIM_DIR)/tb_cpu.v \
                      $(RTL_DIR)/cpu_core.v \
                      $(RTL_DIR)/alu64.v \
                      $(RTL_DIR)/regfile64.v \
                      $(RTL_DIR)/memory64.v
	@mkdir -p $(BUILD_DIR)
	$(IVERILOG) -o $@ $^

# ── Python simulation tests ───────────────────────────────────────────────────
test-python:
	@echo "Running Python emulator tests..."
	$(PYTEST) $(SIM_PY)/tests/ -v

# ── C HAL / driver tests ──────────────────────────────────────────────────────
cmake-build:
	@mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && $(CMAKE) .. -DCMAKE_BUILD_TYPE=Release
	$(CMAKE) --build $(BUILD_DIR) --parallel

test-c: cmake-build
	cd $(BUILD_DIR) && ctest --output-on-failure

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.vcd" -delete
	find . -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true
	find . -name "*.pyc" -delete

# ── Help ──────────────────────────────────────────────────────────────────────
help:
	@echo "Hybrid-64 build targets:"
	@echo "  all          Build C code and run Python tests (default)"
	@echo "  sim-alu      Compile and run Verilog ALU testbench"
	@echo "  sim-cpu      Compile and run Verilog CPU testbench"
	@echo "  test-python  Run Python emulator unit tests"
	@echo "  cmake-build  Configure and build C software layer"
	@echo "  test-c       Run C HAL unit tests"
	@echo "  clean        Remove all build artefacts"
