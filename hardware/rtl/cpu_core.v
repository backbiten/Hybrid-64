// cpu_core.v — Hybrid-64 CPU Core (5-stage pipeline)
// Stages: IF → ID → EX → MEM → WB
`timescale 1ns / 1ps

module cpu_core (
    input  wire        clk,
    input  wire        rst_n,       // Active-low reset
    // Instruction memory interface
    output wire [63:0] imem_addr,
    input  wire [31:0] imem_data,
    input  wire        imem_valid,
    // Data memory interface
    output wire [63:0] dmem_addr,
    output wire [63:0] dmem_wdata,
    output wire [7:0]  dmem_we,     // Byte-enable write strobe
    input  wire [63:0] dmem_rdata,
    input  wire        dmem_valid,
    // Debug / status outputs
    output wire [63:0] dbg_pc,
    output wire        dbg_halted
);

// ─── Instruction encoding ────────────────────────────────────────────────────
// [31:26] opcode  [25:21] rs1  [20:16] rs2  [15:11] rd  [10:0] imm11
localparam OP_NOP  = 6'h00;
localparam OP_ADD  = 6'h01;
localparam OP_SUB  = 6'h02;
localparam OP_AND  = 6'h03;
localparam OP_OR   = 6'h04;
localparam OP_XOR  = 6'h05;
localparam OP_SHL  = 6'h06;
localparam OP_SHR  = 6'h07;
localparam OP_LD   = 6'h08;
localparam OP_ST   = 6'h09;
localparam OP_ADDI = 6'h0A;
localparam OP_JMP  = 6'h0B;
localparam OP_BEQ  = 6'h0C;
localparam OP_BNE  = 6'h0D;
localparam OP_HALT = 6'h3F;

// ─── Pipeline registers ──────────────────────────────────────────────────────
reg [63:0] if_pc,  id_pc,  ex_pc,  mem_pc;
reg [31:0] if_ir,  id_ir;
reg [63:0] ex_alu_a, ex_alu_b;
reg [63:0] ex_result, mem_result;
reg [5:0]  id_op;
reg [4:0]  id_rd,  ex_rd,  mem_rd,  wb_rd;
reg        id_we,  ex_we,  mem_we,  wb_we;
reg [63:0] wb_data;
reg        halted;

// ─── Register file connections ───────────────────────────────────────────────
wire [4:0]  rs1_addr = id_ir[25:21];
wire [4:0]  rs2_addr = id_ir[20:16];
wire [63:0] rs1_data, rs2_data;
wire [63:0] imm_sext = {{53{id_ir[10]}}, id_ir[10:0]};

regfile64 u_regs (
    .clk     (clk),
    .rst_n   (rst_n),
    .ra_addr (rs1_addr),
    .ra_data (rs1_data),
    .rb_addr (rs2_addr),
    .rb_data (rs2_data),
    .we      (wb_we),
    .wr_addr (wb_rd),
    .wr_data (wb_data)
);

// ─── ALU connections ─────────────────────────────────────────────────────────
wire [3:0]  alu_op;
wire [63:0] alu_result;
wire        alu_zero, alu_carry, alu_overflow;

alu64 u_alu (
    .a        (ex_alu_a),
    .b        (ex_alu_b),
    .op       (alu_op),
    .result   (alu_result),
    .zero     (alu_zero),
    .carry    (alu_carry),
    .overflow (alu_overflow)
);

// Map CPU opcode → ALU opcode
function [3:0] cpu_to_alu;
    input [5:0] cpu_op;
    case (cpu_op)
        OP_ADD, OP_ADDI, OP_LD, OP_ST: cpu_to_alu = 4'h0; // ADD
        OP_SUB:                          cpu_to_alu = 4'h1;
        OP_AND:                          cpu_to_alu = 4'h2;
        OP_OR:                           cpu_to_alu = 4'h3;
        OP_XOR:                          cpu_to_alu = 4'h4;
        OP_SHL:                          cpu_to_alu = 4'h6;
        OP_SHR:                          cpu_to_alu = 4'h7;
        default:                         cpu_to_alu = 4'h0;
    endcase
endfunction

assign alu_op = cpu_to_alu(id_op);

// ─── PC / instruction fetch ──────────────────────────────────────────────────
reg [63:0] pc;
assign imem_addr = pc;
assign dbg_pc    = pc;
assign dbg_halted = halted;

// ─── Pipeline control ────────────────────────────────────────────────────────
always @(posedge clk) begin
    if (!rst_n) begin
        pc         <= 64'h0;
        halted     <= 1'b0;
        if_pc      <= 64'h0;
        if_ir      <= 32'h0;
        id_pc      <= 64'h0;
        id_ir      <= 32'h0;
        id_op      <= 6'h0;
        id_rd      <= 5'h0;
        id_we      <= 1'b0;
        ex_rd      <= 5'h0;
        ex_we      <= 1'b0;
        ex_result  <= 64'h0;
        mem_rd     <= 5'h0;
        mem_we     <= 1'b0;
        mem_result <= 64'h0;
        wb_rd      <= 5'h0;
        wb_we      <= 1'b0;
        wb_data    <= 64'h0;
    end else if (!halted) begin

        // ── WB stage ──────────────────────────────────────────────────────
        wb_rd   <= mem_rd;
        wb_we   <= mem_we;
        wb_data <= mem_result;

        // ── MEM stage ─────────────────────────────────────────────────────
        mem_rd     <= ex_rd;
        mem_we     <= ex_we;
        mem_result <= (id_op == OP_LD) ? dmem_rdata : ex_result;

        // ── EX stage ──────────────────────────────────────────────────────
        ex_rd  <= id_rd;
        ex_we  <= id_we;
        ex_result <= alu_result;

        // ── ID stage ──────────────────────────────────────────────────────
        id_pc  <= if_pc;
        id_ir  <= if_ir;
        id_op  <= if_ir[31:26];
        id_rd  <= if_ir[15:11];
        id_we  <= (if_ir[31:26] != OP_NOP  &&
                   if_ir[31:26] != OP_ST   &&
                   if_ir[31:26] != OP_JMP  &&
                   if_ir[31:26] != OP_BEQ  &&
                   if_ir[31:26] != OP_BNE  &&
                   if_ir[31:26] != OP_HALT);

        // ── IF stage ──────────────────────────────────────────────────────
        if_pc  <= pc;
        if_ir  <= imem_valid ? imem_data : 32'h0;

        // Branch / jump resolution (simple, non-speculative)
        if (if_ir[31:26] == OP_HALT) begin
            halted <= 1'b1;
        end else if (if_ir[31:26] == OP_JMP) begin
            pc <= pc + {{43{if_ir[20]}}, if_ir[20:0]};
        end else if (if_ir[31:26] == OP_BEQ && alu_zero) begin
            pc <= pc + imm_sext;
        end else if (if_ir[31:26] == OP_BNE && !alu_zero) begin
            pc <= pc + imm_sext;
        end else begin
            pc <= pc + 64'h4;
        end
    end
end

// ── EX operand select ─────────────────────────────────────────────────────
always @(*) begin
    case (id_op)
        OP_ADDI, OP_LD, OP_ST: begin
            ex_alu_a = rs1_data;
            ex_alu_b = imm_sext;
        end
        default: begin
            ex_alu_a = rs1_data;
            ex_alu_b = rs2_data;
        end
    endcase
end

// ── Data memory interface ─────────────────────────────────────────────────
assign dmem_addr  = ex_result;
assign dmem_wdata = rs2_data;
assign dmem_we    = (id_op == OP_ST) ? 8'hFF : 8'h00;

endmodule
