`timescale 1ns / 1ps

typedef enum logic [2:0] {
    ADD     = 3'b000,
    SUB     = 3'b001,
    MUL     = 3'b010,
    DIV     = 3'b011,
    BIT_AND = 3'b100,
    BIT_OR  = 3'b101,
    BIT_XOR = 3'b110
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
    logic signed [63:0] signed_alu_a, signed_alu_b;

    assign signed_alu_a = alu_a;
    assign signed_alu_b = alu_b;

    always_comb case (alu_op)
        ADD: alu_out = alu_a + alu_b;
        SUB: alu_out = alu_a - alu_b;
        MUL: alu_out = alu_a * alu_b;
        DIV: alu_out = alu_a / alu_b; // todo: consider optimizing this, or multicycle state machine
        BIT_AND: alu_out = alu_a & alu_b;
        BIT_OR:  alu_out = alu_a | alu_b;
        BIT_XOR: alu_out = alu_a ^ alu_b;
        default: alu_out = 0;
    endcase
endmodule
