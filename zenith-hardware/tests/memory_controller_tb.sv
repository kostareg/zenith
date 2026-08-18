`timescale 1ns / 1ps

module memory_controller_tb #(
    parameter bit finish_on_done = 1'b1
);

    reg clock = 0;
    always #5 clock = ~clock;

    logic mem_r = 1'b0;
    logic mem_w = 1'b0;
    logic [1:0] mem_width = 2'b00;
    logic [63:0] mem_addr = 64'd0;
    logic [63:0] mem_in = 64'd0;
    logic [63:0] mem_out;
    logic mem_ready = 1'b0;
    int failures = 0;

    memory_controller dut (
        .clock(clock),
        .mem_r(mem_r),
        .mem_w(mem_w),
        .mem_width(mem_width),
        .mem_addr(mem_addr),
        .mem_in(mem_in),
        .mem_out(mem_out),
        .mem_ready(mem_ready)
    );

    task automatic write_mem(
        input string name,
        input logic [63:0] address,
        input logic [1:0] width,
        input logic [63:0] value
    );
        begin
            mem_addr = address;
            mem_width = width;
            mem_in = value;
            mem_r = 1'b0;
            mem_w = 1'b1;
            @(posedge clock);
            #1;

            if (mem_ready !== 1'b1) begin
                $error("%s write did not assert mem_ready", name);
                failures++;
            end

            mem_w = 1'b0;
            @(posedge clock);
            #1;
        end
    endtask

    task automatic check_read(
        input string name,
        input logic [63:0] address,
        input logic [1:0] width,
        input logic [63:0] expected
    );
        begin
            mem_addr = address;
            mem_width = width;
            mem_r = 1'b1;
            mem_w = 1'b0;
            @(posedge clock);
            #1;

            if (mem_ready !== 1'b1) begin
                $error("%s read did not assert mem_ready", name);
                failures++;
            end

            if (mem_out !== expected) begin
                $error(
                    "%s failed: address=0x%016h width=%0d expected=0x%016h actual=0x%016h",
                    name,
                    address,
                    width,
                    expected,
                    mem_out
                );
                failures++;
            end

            mem_r = 1'b0;
            @(posedge clock);
            #1;
        end
    endtask

    initial begin
        #1;
        if (mem_ready !== 1'b0) begin
            $error("idle mem_ready should start low");
            failures++;
        end

        write_mem("64-bit pattern", 64'd64, 2'b11, 64'h0123_4567_89AB_CDEF);
        check_read("read low byte", 64'd64, 2'b00, 64'h0000_0000_0000_00EF);
        check_read("read low halfword", 64'd64, 2'b01, 64'h0000_0000_0000_CDEF);
        check_read("read low word", 64'd64, 2'b10, 64'h0000_0000_89AB_CDEF);
        check_read("read full doubleword", 64'd64, 2'b11, 64'h0123_4567_89AB_CDEF);

        write_mem("clear partial-store area", 64'd80, 2'b11, 64'hFFFF_FFFF_FFFF_FFFF);
        write_mem("8-bit partial store", 64'd80, 2'b00, 64'h0000_0000_0000_0012);
        check_read("8-bit store preserves upper bytes", 64'd80, 2'b11, 64'hFFFF_FFFF_FFFF_FF12);

        write_mem("16-bit partial store", 64'd82, 2'b01, 64'h0000_0000_0000_3456);
        check_read("16-bit store preserves neighbors", 64'd80, 2'b11, 64'hFFFF_FFFF_3456_FF12);

        write_mem("32-bit partial store", 64'd84, 2'b10, 64'h0000_0000_A1B2_C3D4);
        check_read("32-bit store preserves lower bytes", 64'd80, 2'b11, 64'hA1B2_C3D4_3456_FF12);

        if (failures == 0)
            $display("All memory controller tests passed.");
        else
            $error("%0d memory controller test(s) failed.", failures);

        if (finish_on_done)
            $finish;
    end

endmodule
