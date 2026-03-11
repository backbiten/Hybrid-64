# Hybrid-64 FPGA Constraints (Xilinx XDC format)
# Target: Xilinx Artix-7 (XC7A35T) — adaptable to other families
# Adjust pin locations and I/O standards for your board.

# ── Clock ────────────────────────────────────────────────────────────────────
# Primary 100 MHz system clock input
set_property PACKAGE_PIN W5      [get_ports clk]
set_property IOSTANDARD  LVCMOS33 [get_ports clk]
create_clock -name sys_clk -period 10.000 [get_ports clk]

# ── Reset (active-low, push-button) ──────────────────────────────────────────
set_property PACKAGE_PIN U18     [get_ports rst_n]
set_property IOSTANDARD  LVCMOS33 [get_ports rst_n]
set_false_path -from [get_ports rst_n]

# ── UART (debug console) ──────────────────────────────────────────────────────
set_property PACKAGE_PIN A18     [get_ports uart_tx]
set_property IOSTANDARD  LVCMOS33 [get_ports uart_tx]
set_property PACKAGE_PIN B18     [get_ports uart_rx]
set_property IOSTANDARD  LVCMOS33 [get_ports uart_rx]

# ── LEDs (status indicators) ──────────────────────────────────────────────────
# LED[0] = CPU running, LED[1] = CPU halted, LED[2..3] = status flags
set_property PACKAGE_PIN U16     [get_ports {led[0]}]
set_property PACKAGE_PIN E19     [get_ports {led[1]}]
set_property PACKAGE_PIN U19     [get_ports {led[2]}]
set_property PACKAGE_PIN V19     [get_ports {led[3]}]
set_property IOSTANDARD  LVCMOS33 [get_ports {led[*]}]

# ── JTAG (boundary scan / debug access) ──────────────────────────────────────
# NOTE: Vivado manages JTAG pins automatically; do not override unless
# using a custom debug interface.

# ── Timing exceptions ─────────────────────────────────────────────────────────
# Multi-cycle path for memory initialisation signals (static during operation)
set_multicycle_path -setup 2 -from [get_cells u_mem/*] -to [get_cells u_cpu/*]
set_multicycle_path -hold  1 -from [get_cells u_mem/*] -to [get_cells u_cpu/*]
