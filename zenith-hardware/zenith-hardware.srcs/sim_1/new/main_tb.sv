`timescale 1ns / 1ps

module main_tb;

    logic [3:0] DIG;
    logic [7:0] LED_out;

    main dut (
        .DIG(DIG),
        .LED_out(LED_out)
    );

    initial begin
        #1;

        assert (DIG == 4'b0000)
            else $fatal("DIG wrong: got %b", DIG);

        assert (LED_out == 8'b00000001)
            else $fatal("LED_out wrong: got %b", LED_out);

        $display("PASS: DIG=%b LED_out=%b", DIG, LED_out);

        #10;
        $finish;
    end

endmodule