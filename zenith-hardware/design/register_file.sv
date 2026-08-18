`timescale 1ns / 1ps

/**
 * @brief the register file stores and provides access to all registers
 * @param clock  clock
 * @param rf_w   whether we should write data_w to reg_w
 * @param reg_a  register a select
 * @param reg_b  register b select
 * @param reg_w  register w select
 * @param data_w data to write to reg_w
 * @param data_a register value of a
 * @param data_b register value of b
 */
module register_file(
    input logic clock,
    input logic rf_w,
    input logic [4:0] reg_a,
    input logic [4:0] reg_b,
    input logic [4:0] reg_w,
    input logic [63:0] data_w,
    output logic [63:0] data_a,
    output logic [63:0] data_b
);
    logic [63:0] registers [0:31];

    initial registers[0] = 64'd0;

    assign data_a = registers[reg_a];
    assign data_b = registers[reg_b];

    always@(posedge clock)
        if (rf_w && reg_w != 5'd0)
            registers[reg_w] <= data_w;
endmodule