`timescale 1ns / 1ps

module arithmetic_logic_unit_tb #(
    parameter bit finish_on_done = 1'b1
);

    logic [63:0] alu_a, alu_b, alu_out;
    alu_op_t alu_op;
    int failures = 0;

    arithmetic_logic_unit dut (
        .alu_a(alu_a),
        .alu_b(alu_b),
        .alu_op(alu_op),
        .alu_out(alu_out)
    );

    task automatic check(
        input string name,
        input alu_op_t op,
        input logic [63:0] a,
        input logic [63:0] b,
        input logic [63:0] expected
    );
        begin
            alu_a = a;
            alu_b = b;
            alu_op = op;
            #1;

            if (alu_out !== expected) begin
                $error(
                    "%s failed: a=0x%016h b=0x%016h expected=0x%016h actual=0x%016h",
                    name,
                    a,
                    b,
                    expected,
                    alu_out
                );
                failures++;
            end
        end
    endtask

    initial begin
        check("ADD positive", ADD, 64'd5, 64'd7, 64'd12);
        check("ADD carry", ADD, 64'hFFFF_FFFF_FFFF_FFFF, 64'd1, 64'd0);
        check("ADD signed bits", ADD, 64'hFFFF_FFFF_FFFF_FFF6, 64'd4, 64'hFFFF_FFFF_FFFF_FFFA);

        check("SUB positive", SUB, 64'd20, 64'd8, 64'd12);
        check("SUB underflow", SUB, 64'd3, 64'd10, 64'hFFFF_FFFF_FFFF_FFF9);
        check("SUB zero", SUB, 64'h1234_5678_9ABC_DEF0, 64'h1234_5678_9ABC_DEF0, 64'd0);

        check("MUL positive", MUL, 64'd6, 64'd7, 64'd42);
        check("MUL zero", MUL, 64'd12345, 64'd0, 64'd0);
        check("MUL signed bits", MUL, 64'hFFFF_FFFF_FFFF_FFFC, 64'd3, 64'hFFFF_FFFF_FFFF_FFF4);

        check("DIV positive", DIV, 64'd42, 64'd7, 64'd6);
        check("DIV negative dividend", DIV, 64'hFFFF_FFFF_FFFF_FFD9, 64'd5, 64'hFFFF_FFFF_FFFF_FFF9);
        check("DIV negative divisor", DIV, 64'd45, 64'hFFFF_FFFF_FFFF_FFF7, 64'hFFFF_FFFF_FFFF_FFFB);

        check("BIT_AND mixed", BIT_AND, 64'hF0F0_F0F0_F0F0_F0F0, 64'h0FF0_00FF_FF00_1234, 64'h00F0_00F0_F000_1030);
        check("BIT_AND all ones", BIT_AND, 64'hFFFF_FFFF_FFFF_FFFF, 64'h1234_5678_9ABC_DEF0, 64'h1234_5678_9ABC_DEF0);
        check("BIT_AND sparse", BIT_AND, 64'h8000_0000_0000_0001, 64'h0000_0000_0000_0001, 64'd1);

        check("BIT_OR mixed", BIT_OR, 64'hF0F0_F0F0_F0F0_F0F0, 64'h0FF0_00FF_FF00_1234, 64'hFFF0_F0FF_FFF0_F2F4);
        check("BIT_OR zero", BIT_OR, 64'd0, 64'h1234_5678_9ABC_DEF0, 64'h1234_5678_9ABC_DEF0);
        check("BIT_OR sparse", BIT_OR, 64'h8000_0000_0000_0000, 64'h0000_0000_0000_0001, 64'h8000_0000_0000_0001);

        check("BIT_XOR mixed", BIT_XOR, 64'hF0F0_F0F0_F0F0_F0F0, 64'h0FF0_00FF_FF00_1234, 64'hFF00_F00F_0FF0_E2C4);
        check("BIT_XOR self", BIT_XOR, 64'h1234_5678_9ABC_DEF0, 64'h1234_5678_9ABC_DEF0, 64'd0);
        check("BIT_XOR sparse", BIT_XOR, 64'h8000_0000_0000_0001, 64'h0000_0000_0000_0001, 64'h8000_0000_0000_0000);

        check("BIT_NOT zero", BIT_NOT, 64'd0, 64'd0, 64'hFFFF_FFFF_FFFF_FFFF);
        check("BIT_NOT mixed", BIT_NOT, 64'h1234_5678_9ABC_DEF0, 64'd0, 64'hEDCB_A987_6543_210F);
        check("BIT_NOT sparse", BIT_NOT, 64'h8000_0000_0000_0001, 64'd0, 64'h7FFF_FFFF_FFFF_FFFE);

        check("SLL small", SLL, 64'd1, 64'd3, 64'd8);
        check("SLL overflow", SLL, 64'h8000_0000_0000_0000, 64'd1, 64'd0);
        check("SLL masked amount", SLL, 64'd2, 64'd65, 64'd4);

        check("SRL sign bit", SRL, 64'h8000_0000_0000_0000, 64'd63, 64'd1);
        check("SRL mixed", SRL, 64'hF000_0000_0000_0000, 64'd4, 64'h0F00_0000_0000_0000);
        check("SRL masked amount", SRL, 64'h1234_5678_9ABC_DEF0, 64'd64, 64'h1234_5678_9ABC_DEF0);

        check("SLA small", SLA, 64'd1, 64'd4, 64'd16);
        check("SLA sign bit", SLA, 64'h4000_0000_0000_0000, 64'd1, 64'h8000_0000_0000_0000);
        check("SLA masked amount", SLA, 64'd3, 64'd66, 64'd12);

        check("SRA sign fill", SRA, 64'h8000_0000_0000_0000, 64'd63, 64'hFFFF_FFFF_FFFF_FFFF);
        check("SRA mixed", SRA, 64'hF000_0000_0000_0000, 64'd4, 64'hFF00_0000_0000_0000);
        check("SRA positive", SRA, 64'h7FFF_FFFF_FFFF_FFFE, 64'd1, 64'h3FFF_FFFF_FFFF_FFFF);

        if (failures == 0)
            $display("All ALU tests passed.");
        else
            $error("%0d ALU test(s) failed.", failures);

        if (finish_on_done)
            $finish;
    end

endmodule
