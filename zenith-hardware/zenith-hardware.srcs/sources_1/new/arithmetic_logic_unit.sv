`timescale 1ns / 1ps

typedef enum logic [1:0] {
    ADD = 2'b00,
    SUB = 2'b01,
    MUL = 2'b10,
    DIV = 2'b11
} alu_op_t;

/**
 * @brief arithmetic logic unit
 * @param alu_a   alu input a
 * @param alu_b   alu input b
 * @param alu_op  alu operation
 * @param alu_out alu output
 */
module arithmetic_logic_unit(
    input logic [63:0] alu_a,
    input logic [63:0] alu_b,
    input alu_op_t alu_op,
    output logic [63:0] alu_out
);
    always_comb case (alu_op)
        ADD: alu_out = alu_a + alu_b;
        SUB: alu_out = alu_a - alu_b;
        DIV: alu_out = alu_a / alu_b; // todo: consider optimizing this, or multicycle state machine
        MUL: alu_out = alu_a * alu_b;
        default: alu_out = 0;
    endcase
endmodule
