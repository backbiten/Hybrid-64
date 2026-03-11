// memory64.v — Unified 64-bit memory model (instruction + data)
// Parameterised depth; defaults to 64 KiB (16 K × 32-bit words)
`timescale 1ns / 1ps

module memory64 #(
    parameter MEM_DEPTH = 16384,           // Number of 32-bit words
    parameter ADDR_BITS = $clog2(MEM_DEPTH)
)(
    input  wire        clk,
    input  wire        rst_n,
    // Instruction port (read-only)
    input  wire [63:0] imem_addr,
    output reg  [31:0] imem_data,
    output reg         imem_valid,
    // Data port (read / write, 64-bit wide)
    input  wire [63:0] dmem_addr,
    input  wire [63:0] dmem_wdata,
    input  wire [7:0]  dmem_we,    // Byte-enable write strobe
    output reg  [63:0] dmem_rdata,
    output reg         dmem_valid
);

// Storage: 8-bit bytes, addressed individually
reg [7:0] mem [0:(MEM_DEPTH*4)-1];

integer i;

always @(posedge clk) begin
    if (!rst_n) begin
        for (i = 0; i < MEM_DEPTH*4; i = i + 1)
            mem[i] <= 8'h0;
        imem_valid <= 1'b0;
        dmem_valid <= 1'b0;
    end else begin
        // ── Instruction fetch (little-endian 32-bit read) ────────────────
        imem_data  <= { mem[imem_addr+3], mem[imem_addr+2],
                        mem[imem_addr+1], mem[imem_addr  ] };
        imem_valid <= 1'b1;

        // ── Data read (little-endian 64-bit read) ────────────────────────
        dmem_rdata <= { mem[dmem_addr+7], mem[dmem_addr+6],
                        mem[dmem_addr+5], mem[dmem_addr+4],
                        mem[dmem_addr+3], mem[dmem_addr+2],
                        mem[dmem_addr+1], mem[dmem_addr  ] };
        dmem_valid <= 1'b1;

        // ── Data write (byte-enable) ──────────────────────────────────────
        if (dmem_we[0]) mem[dmem_addr  ] <= dmem_wdata[ 7: 0];
        if (dmem_we[1]) mem[dmem_addr+1] <= dmem_wdata[15: 8];
        if (dmem_we[2]) mem[dmem_addr+2] <= dmem_wdata[23:16];
        if (dmem_we[3]) mem[dmem_addr+3] <= dmem_wdata[31:24];
        if (dmem_we[4]) mem[dmem_addr+4] <= dmem_wdata[39:32];
        if (dmem_we[5]) mem[dmem_addr+5] <= dmem_wdata[47:40];
        if (dmem_we[6]) mem[dmem_addr+6] <= dmem_wdata[55:48];
        if (dmem_we[7]) mem[dmem_addr+7] <= dmem_wdata[63:56];
    end
end

// Helper task: initialise a word at a given byte address
task write_word;
    input [63:0] addr;
    input [31:0] data;
    begin
        mem[addr  ] = data[ 7: 0];
        mem[addr+1] = data[15: 8];
        mem[addr+2] = data[23:16];
        mem[addr+3] = data[31:24];
    end
endtask

endmodule
