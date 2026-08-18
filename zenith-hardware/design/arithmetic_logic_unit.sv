`timescale 1ns / 1ps

typedef enum logic [3:0] {
    ADD     = 4'b0000,
    SUB     = 4'b0001,
    MUL     = 4'b0010,
    DIV     = 4'b0011,
    BIT_AND = 4'b0100,
    BIT_OR  = 4'b0101,
    BIT_XOR = 4'b0110,
    BIT_NOT = 4'b0111,
    SLL     = 4'b1000,
    SRL     = 4'b1001,
    SLA     = 4'b1010,
    SRA     = 4'b1011
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
    logic [5:0] shift_amount;

    assign signed_alu_a = alu_a;
    assign signed_alu_b = alu_b;
    assign shift_amount = alu_b[5:0];

    always_comb case (alu_op)
        ADD: alu_out = alu_a + alu_b;
        SUB: alu_out = alu_a - alu_b;
        MUL: alu_out = alu_a * alu_b;
        DIV: begin
            // todo: consider optimizing this, or multicycle state machine
            if (alu_b == 64'd0)
                alu_out = 64'd0;
            else if (alu_a == 64'h8000_0000_0000_0000 && alu_b == 64'hFFFF_FFFF_FFFF_FFFF)
                alu_out = alu_a;
            else
                alu_out = signed_alu_a / signed_alu_b;
        end
        BIT_AND: alu_out = alu_a & alu_b;
        BIT_OR:  alu_out = alu_a | alu_b;
        BIT_XOR: alu_out = alu_a ^ alu_b;
        BIT_NOT: alu_out = ~alu_a;
        SLL:     alu_out = alu_a << shift_amount;
        SRL:     alu_out = alu_a >> shift_amount;
        SLA:     alu_out = alu_a <<< shift_amount;
        SRA:     alu_out = signed_alu_a >>> shift_amount;
        default: alu_out = 0;
    endcase
endmodule
