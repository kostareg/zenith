`timescale 1ns / 1ps

typedef enum logic [2:0] {
    FETCH = 3'b000,
    FETCH_WAIT = 3'b001,
    DECODE = 3'b010,
    EXECUTE = 3'b011,
    CYCLE = 3'b100
} state_t;

/**
 * @brief top level processor
 * @param clock   clock
 * @param digit   selected digit to write to
 * @param led_out selected 7-segment value
 */
module main (
    input logic clock,
    output logic [3:0] digit,
    output logic [7:0] led_out
);
    logic [63:0] pc = 0;
    logic [31:0] instruction;

    // register file
    logic rf_w;
    logic [4:0] reg_a, reg_b, reg_w;
    logic [63:0] data_w, data_a, data_b;

    // memory controller
    logic mem_r, mem_w, mem_ready;
    logic [63:0] mem_addr, mem_in, mem_out;

    // alu
    logic [63:0] alu_a, alu_b, alu_out;
    alu_op_t alu_op;

    state_t state;
    register_file rf (
        .clock(clock),
        .rf_w(rf_w),
        .reg_a(reg_a),
        .reg_b(reg_b),
        .reg_w(reg_w),
        .data_w(data_w),
        .data_a(data_a),
        .data_b(data_b)
    );
    memory_controller mem (
        .clock(clock),
        .mem_r(mem_r),
        .mem_w(mem_w),
        .mem_addr(mem_addr),
        .mem_in(mem_in),
        .mem_out(mem_out),
        .mem_ready(mem_ready)
    );
    arithmetic_logic_unit alu (
        .alu_a(alu_a),
        .alu_b(alu_b),
        .alu_op(alu_op),
        .alu_out(alu_out)
    );

    always@(posedge clock) case (state)
        FETCH: begin
            mem_addr <= pc;
            mem_r <= 1'b1;
            state <= FETCH_WAIT;
        end

        FETCH_WAIT: begin
            if (mem_ready) begin
                instruction <= mem_out[31:0];
                // todo: incorporate ALU
                pc <= pc + 4;
                mem_r <= 1'b0;
                state <= DECODE;
            end
        end

        DECODE:  state <= EXECUTE;
        EXECUTE: state <= CYCLE;
        CYCLE:   state <= FETCH;
        default: state <= FETCH;
    endcase
endmodule
