`timescale 1ns / 1ps

module main_tb;

    reg clock = 0;
    always #5 clock = ~clock;
    logic [3:0] digit;
    logic [7:0] led_out;

    main dut (
        .clock(clock),
        .digit(digit),
        .led_out(led_out)
    );

    arithmetic_logic_unit_tb #(
        .finish_on_done(1'b0)
    ) alu_tests();

    memory_controller_tb #(
        .finish_on_done(1'b0)
    ) memory_tests();

    keyboard_tb #(
        .finish_on_done(1'b0)
    ) keyboard_tests();

    control_flow_tb #(
        .finish_on_done(1'b0)
    ) control_tests();

    initial begin
        #50000;
        $finish;
    end

endmodule
