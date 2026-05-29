`timescale 1ns / 1ps

import zenith_instructions::*;

typedef enum logic [2:0] {
    FETCH = 3'b000,
    FETCH_WAIT = 3'b001,
    DECODE = 3'b010,
    EXECUTE = 3'b011
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
    logic signed [14:0] imm15;
    logic [63:0] imm15_extended;

    // memory controller
    logic mem_r, mem_w, mem_ready;
    logic [63:0] mem_addr, mem_in, mem_out;

    // alu
    logic [63:0] alu_a, alu_b, alu_out;
    alu_op_t alu_op;
    logic alu_a_select;
    logic [1:0] alu_b_select;
    assign alu_a = alu_a_select ? pc : data_a;
    assign alu_b = alu_b_select[1] ? 64'd4 :
                   alu_b_select[0] ? imm15_extended : data_b;
    assign led_out = alu_out; // todo: remove

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

    assign imm15_extended = {{49{imm15[14]}}, imm15};

    always@(posedge clock) case (state)
        FETCH: begin
            rf_w <= 1'b0;
            mem_addr <= pc;
            mem_r <= 1'b1;
            alu_a_select <= 1'b1;
            alu_b_select <= 2'b10;
            alu_op <= ADD;
            state <= FETCH_WAIT;
        end

        FETCH_WAIT: begin
            if (mem_ready) begin
                instruction <= mem_out[31:0];
                pc <= alu_out;
                mem_r <= 1'b0;
                state <= DECODE;
            end
        end

        DECODE: begin
            case (instruction[6:0])
                INSTR_ADD: begin
                    reg_w <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    reg_b <= instruction[21:17];
                    alu_op <= ADD;
                    alu_a_select <= 1'b0;
                    alu_b_select <= 2'b00;
                end
                INSTR_ADDI: begin
                    reg_w <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    imm15 <= instruction[31:17];
                    alu_op <= ADD;
                    alu_a_select <= 1'b0;
                    alu_b_select <= 2'b01;
                end
            endcase
            state <= EXECUTE;
        end
        EXECUTE: begin
            case (instruction[6:0])
                INSTR_ADD, INSTR_ADDI: begin
                    data_w <= alu_out;
                    rf_w <= 1'b1;
                end
            endcase
            state <= FETCH;
        end
        default: state <= FETCH;
    endcase
endmodule
