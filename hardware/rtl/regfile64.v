// regfile64.v — 64-bit Register File (32 general-purpose registers)
// Part of the Hybrid-64 architecture
`timescale 1ns / 1ps

module regfile64 #(
    parameter NUM_REGS = 32,
    parameter REG_BITS = $clog2(NUM_REGS)
)(
    input  wire              clk,
    input  wire              rst_n,    // Active-low synchronous reset
    // Read port A
    input  wire [REG_BITS-1:0] ra_addr,
    output wire [63:0]         ra_data,
    // Read port B
    input  wire [REG_BITS-1:0] rb_addr,
    output wire [63:0]         rb_data,
    // Write port
    input  wire              we,
    input  wire [REG_BITS-1:0] wr_addr,
    input  wire [63:0]         wr_data
);

reg [63:0] regs [0:NUM_REGS-1];

integer i;

// Synchronous write with synchronous reset
always @(posedge clk) begin
    if (!rst_n) begin
        for (i = 0; i < NUM_REGS; i = i + 1)
            regs[i] <= 64'h0;
    end else if (we && (wr_addr != 0)) begin
        // Register 0 is hardwired to zero
        regs[wr_addr] <= wr_data;
    end
end

// Asynchronous reads; register 0 always reads as zero
assign ra_data = (ra_addr == 0) ? 64'h0 : regs[ra_addr];
assign rb_data = (rb_addr == 0) ? 64'h0 : regs[rb_addr];

endmodule
