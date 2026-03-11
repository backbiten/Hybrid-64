// tb_alu64.v — Directed testbench for the 64-bit ALU
`timescale 1ns / 1ps

module tb_alu64;

reg  [63:0] a, b;
reg  [3:0]  op;
wire [63:0] result;
wire        zero, carry, overflow;

alu64 dut (
    .a        (a),
    .b        (b),
    .op       (op),
    .result   (result),
    .zero     (zero),
    .carry    (carry),
    .overflow (overflow)
);

integer pass_count, fail_count;

task check64;
    input [63:0] got, expected;
    input [127:0] name;
    begin
        if (got !== expected) begin
            $display("FAIL %-20s: got 0x%016h expected 0x%016h", name, got, expected);
            fail_count = fail_count + 1;
        end else begin
            $display("PASS %-20s", name);
            pass_count = pass_count + 1;
        end
    end
endtask

task check1;
    input got, expected;
    input [127:0] name;
    begin
        if (got !== expected) begin
            $display("FAIL %-20s: got %b expected %b", name, got, expected);
            fail_count = fail_count + 1;
        end else begin
            $display("PASS %-20s", name);
            pass_count = pass_count + 1;
        end
    end
endtask

initial begin
    pass_count = 0;
    fail_count = 0;

    // ADD
    a = 64'd100; b = 64'd200; op = 4'h0; #10;
    check64(result, 64'd300,   "ADD 100+200");
    check1(zero, 0,            "ADD zero=0");

    // ADD with carry
    a = 64'hFFFFFFFFFFFFFFFF; b = 64'h1; op = 4'h0; #10;
    check64(result, 64'h0,     "ADD overflow→0");
    check1(zero, 1,            "ADD zero flag");
    check1(carry, 1,           "ADD carry flag");

    // SUB
    a = 64'd500; b = 64'd123; op = 4'h1; #10;
    check64(result, 64'd377,   "SUB 500-123");

    // AND
    a = 64'hFF00FF00FF00FF00; b = 64'h0F0F0F0F0F0F0F0F; op = 4'h2; #10;
    check64(result, 64'h0F000F000F000F00, "AND");

    // OR
    a = 64'hAA00AA00AA00AA00; b = 64'h0055005500550055; op = 4'h3; #10;
    check64(result, 64'hAA55AA55AA55AA55, "OR");

    // XOR
    a = 64'hDEADBEEFDEADBEEF; b = 64'hDEADBEEFDEADBEEF; op = 4'h4; #10;
    check64(result, 64'h0,     "XOR same→0");
    check1(zero, 1,            "XOR zero flag");

    // NOT
    a = 64'h0000000000000000; b = 64'h0; op = 4'h5; #10;
    check64(result, 64'hFFFFFFFFFFFFFFFF, "NOT 0");

    // SHL
    a = 64'h1; b = 64'd4; op = 4'h6; #10;
    check64(result, 64'h10,    "SHL 1<<4");

    // SHR
    a = 64'h100; b = 64'd4; op = 4'h7; #10;
    check64(result, 64'h10,    "SHR 0x100>>4");

    // MUL
    a = 64'd7; b = 64'd6; op = 4'h9; #10;
    check64(result, 64'd42,    "MUL 7*6");

    // DIV
    a = 64'd100; b = 64'd7; op = 4'hA; #10;
    check64(result, 64'd14,    "DIV 100/7");

    // DIV by zero
    a = 64'd100; b = 64'd0; op = 4'hA; #10;
    check64(result, 64'hFFFFFFFFFFFFFFFF, "DIV by zero");

    // MOD
    a = 64'd100; b = 64'd7; op = 4'hB; #10;
    check64(result, 64'd2,     "MOD 100%7");

    // CMP equal
    a = 64'd42; b = 64'd42; op = 4'hC; #10;
    check64(result, 64'h0,     "CMP equal");

    // CMP greater
    a = 64'd50; b = 64'd42; op = 4'hC; #10;
    check64(result, 64'h1,     "CMP greater");

    // Summary
    $display("─────────────────────────────────");
    $display("Results: %0d passed, %0d failed", pass_count, fail_count);
    if (fail_count == 0)
        $display("ALL TESTS PASSED");
    $finish;
end

endmodule
