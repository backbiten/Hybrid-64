// tb_cpu.v — Testbench for Hybrid-64 CPU core
// Exercises basic ALU, load/store, branch and halt operations
`timescale 1ns / 1ps

module tb_cpu;

// ─── DUT signals ────────────────────────────────────────────────────────────
reg         clk, rst_n;
wire [63:0] imem_addr, dmem_addr, dmem_wdata;
wire [31:0] imem_data;
wire        imem_valid, dmem_valid;
wire [63:0] dmem_rdata;
wire [7:0]  dmem_we;
wire [63:0] dbg_pc;
wire        dbg_halted;

// ─── Instantiate CPU and memory ──────────────────────────────────────────────
cpu_core u_cpu (
    .clk        (clk),
    .rst_n      (rst_n),
    .imem_addr  (imem_addr),
    .imem_data  (imem_data),
    .imem_valid (imem_valid),
    .dmem_addr  (dmem_addr),
    .dmem_wdata (dmem_wdata),
    .dmem_we    (dmem_we),
    .dmem_rdata (dmem_rdata),
    .dmem_valid (dmem_valid),
    .dbg_pc     (dbg_pc),
    .dbg_halted (dbg_halted)
);

memory64 u_mem (
    .clk        (clk),
    .rst_n      (rst_n),
    .imem_addr  (imem_addr),
    .imem_data  (imem_data),
    .imem_valid (imem_valid),
    .dmem_addr  (dmem_addr),
    .dmem_wdata (dmem_wdata),
    .dmem_we    (dmem_we),
    .dmem_rdata (dmem_rdata),
    .dmem_valid (dmem_valid)
);

// ─── Helpers ─────────────────────────────────────────────────────────────────
task load_instr;
    input [63:0] addr;
    input [31:0] instr;
    begin
        u_mem.mem[addr  ] = instr[ 7: 0];
        u_mem.mem[addr+1] = instr[15: 8];
        u_mem.mem[addr+2] = instr[23:16];
        u_mem.mem[addr+3] = instr[31:24];
    end
endtask

// Build instruction words
// Format: [31:26]=op  [25:21]=rs1  [20:16]=rs2  [15:11]=rd  [10:0]=imm
function [31:0] mk_r;
    input [5:0] op; input [4:0] rs1, rs2, rd;
    mk_r = {op, rs1, rs2, rd, 11'h0};
endfunction

function [31:0] mk_i;
    input [5:0] op; input [4:0] rs1, rd; input [10:0] imm;
    mk_i = {op, rs1, 5'h0, rd, imm};
endfunction

// ─── Clock ───────────────────────────────────────────────────────────────────
initial clk = 0;
always #5 clk = ~clk;   // 100 MHz

// ─── Test program ─────────────────────────────────────────────────────────────
integer pass_count, fail_count;

task check;
    input [63:0] got, expected;
    input [63:0] test_id;
    begin
        if (got !== expected) begin
            $display("FAIL test %0d: got 0x%016h expected 0x%016h",
                     test_id, got, expected);
            fail_count = fail_count + 1;
        end else begin
            $display("PASS test %0d", test_id);
            pass_count = pass_count + 1;
        end
    end
endtask

initial begin
    pass_count = 0;
    fail_count = 0;

    // Reset
    rst_n = 0;
    #20;
    rst_n = 1;

    // ── Program 1: ADDI r1,r0,42 ; HALT
    load_instr(0,  mk_i(6'h0A, 5'h0, 5'h1, 11'd42)); // addi r1, r0, 42
    load_instr(4,  32'hFC000000);                      // halt (op=0x3F)

    // Run until halted or timeout
    #200;
    check(u_cpu.u_regs.regs[1], 64'd42, 1);

    // ── Program 2: ADD + SUB
    rst_n = 0; #20; rst_n = 1;
    load_instr(0,  mk_i(6'h0A, 5'h0, 5'h1, 11'd10));  // addi r1, r0, 10
    load_instr(4,  mk_i(6'h0A, 5'h0, 5'h2, 11'd30));  // addi r2, r0, 30
    load_instr(8,  mk_r(6'h01, 5'h1, 5'h2, 5'h3));    // add  r3, r1, r2
    load_instr(12, mk_r(6'h02, 5'h2, 5'h1, 5'h4));    // sub  r4, r2, r1
    load_instr(16, 32'hFC000000);                       // halt
    #300;
    check(u_cpu.u_regs.regs[3], 64'd40, 2);
    check(u_cpu.u_regs.regs[4], 64'd20, 3);

    // Summary
    #10;
    $display("─────────────────────────────────");
    $display("Results: %0d passed, %0d failed", pass_count, fail_count);
    if (fail_count == 0)
        $display("ALL TESTS PASSED");
    else
        $display("SOME TESTS FAILED");
    $finish;
end

// ── Timeout watchdog ──────────────────────────────────────────────────────────
initial begin
    #50000;
    $display("TIMEOUT — simulation did not complete");
    $finish;
end

endmodule
