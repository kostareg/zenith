`timescale 1ns / 1ps

module main_tb;

    reg CLOCK = 0;
    always #5 CLOCK = ~CLOCK;
    logic [3:0] DIG;
    logic [7:0] LED_out;

    main dut (
        .CLOCK(CLOCK),
        .DIG(DIG),
        .LED_out(LED_out)
    );

    initial begin
        #1;

        #100;
        $finish;
    end

endmodule