// alu64.v — 64-bit Arithmetic Logic Unit
// Part of the Hybrid-64 architecture
`timescale 1ns / 1ps

module alu64 (
    input  wire [63:0] a,       // Operand A
    input  wire [63:0] b,       // Operand B
    input  wire [3:0]  op,      // Operation select
    output reg  [63:0] result,  // ALU result
    output reg         zero,    // Zero flag
    output reg         carry,   // Carry flag
    output reg         overflow // Overflow flag
);

// ALU operation codes
localparam ALU_ADD  = 4'h0;
localparam ALU_SUB  = 4'h1;
localparam ALU_AND  = 4'h2;
localparam ALU_OR   = 4'h3;
localparam ALU_XOR  = 4'h4;
localparam ALU_NOT  = 4'h5;
localparam ALU_SHL  = 4'h6;
localparam ALU_SHR  = 4'h7;
localparam ALU_SAR  = 4'h8;
localparam ALU_MUL  = 4'h9;
localparam ALU_DIV  = 4'hA;
localparam ALU_MOD  = 4'hB;
localparam ALU_CMP  = 4'hC;
localparam ALU_NAND = 4'hD;
localparam ALU_NOR  = 4'hE;
localparam ALU_XNOR = 4'hF;

reg [64:0] wide_result;  // Extra bit for carry detection

always @(*) begin
    carry    = 1'b0;
    overflow = 1'b0;
    wide_result = 65'b0;

    case (op)
        ALU_ADD: begin
            wide_result = {1'b0, a} + {1'b0, b};
            result      = wide_result[63:0];
            carry       = wide_result[64];
            overflow    = (~a[63] & ~b[63] & result[63]) |
                          ( a[63] &  b[63] & ~result[63]);
        end
        ALU_SUB: begin
            wide_result = {1'b0, a} - {1'b0, b};
            result      = wide_result[63:0];
            carry       = wide_result[64];
            overflow    = (~a[63] &  b[63] & result[63]) |
                          ( a[63] & ~b[63] & ~result[63]);
        end
        ALU_AND:  result = a & b;
        ALU_OR:   result = a | b;
        ALU_XOR:  result = a ^ b;
        ALU_NOT:  result = ~a;
        ALU_SHL:  result = a << b[5:0];
        ALU_SHR:  result = a >> b[5:0];
        ALU_SAR:  result = $signed(a) >>> b[5:0];
        ALU_MUL:  result = a * b;
        ALU_DIV:  result = (b != 0) ? a / b : 64'hFFFFFFFFFFFFFFFF;
        ALU_MOD:  result = (b != 0) ? a % b : 64'h0;
        ALU_CMP: begin
            result = (a == b) ? 64'h0 :
                     (a >  b) ? 64'h1 : 64'hFFFFFFFFFFFFFFFF;
        end
        ALU_NAND: result = ~(a & b);
        ALU_NOR:  result = ~(a | b);
        ALU_XNOR: result = ~(a ^ b);
        default:  result = 64'h0;
    endcase

    zero = (result == 64'h0);
end

endmodule
