`timescale 1ns / 1ps

module keyboard_tb #(
    parameter bit finish_on_done = 1'b1
);

    localparam logic [3:0] STATUS_CONTROL = 4'h0;
    localparam logic [3:0] KEY_EVENT      = 4'h4;
    localparam logic [3:0] FIFO_COUNT     = 4'h8;

    localparam logic [31:0] STATUS_READY      = 32'h0000_0001;
    localparam logic [31:0] STATUS_FULL       = 32'h0000_0002;
    localparam logic [31:0] STATUS_OVERFLOW   = 32'h0000_0004;
    localparam logic [31:0] STATUS_ENABLE     = 32'h0000_0008;
    localparam logic [31:0] STATUS_CLEAR_FIFO = 32'h0000_0010;
    localparam logic [31:0] EXPECTED_EVENT    = 32'h0001_0005;

    reg clock = 0;
    always #5 clock = ~clock;

    logic reset = 1'b1;
    logic ps2_clock = 1'b1;
    logic ps2_data = 1'b1;

    logic s_apb_psel = 1'b0;
    logic s_apb_penable = 1'b0;
    logic s_apb_pwrite = 1'b0;
    logic [3:0] s_apb_paddr = 4'h0;
    logic [31:0] s_apb_pwdata = 32'h0;
    logic s_apb_pready;
    logic [31:0] s_apb_prdata;
    logic s_apb_pslverr;

    int failures = 0;

    keyboard dut (
        .clock(clock),
        .reset(reset),
        .ps2_clock(ps2_clock),
        .ps2_data(ps2_data),
        .s_apb_psel(s_apb_psel),
        .s_apb_penable(s_apb_penable),
        .s_apb_pwrite(s_apb_pwrite),
        .s_apb_paddr(s_apb_paddr),
        .s_apb_pwdata(s_apb_pwdata),
        .s_apb_pready(s_apb_pready),
        .s_apb_prdata(s_apb_prdata),
        .s_apb_pslverr(s_apb_pslverr)
    );

    task automatic reset_dut();
        begin
            reset = 1'b1;
            s_apb_psel = 1'b0;
            s_apb_penable = 1'b0;
            s_apb_pwrite = 1'b0;
            s_apb_paddr = 4'h0;
            s_apb_pwdata = 32'h0;
            ps2_clock = 1'b1;
            ps2_data = 1'b1;
            repeat (5) @(posedge clock);
            reset = 1'b0;
            repeat (2) @(posedge clock);
        end
    endtask

    task automatic apb_write(
        input logic [3:0] addr,
        input logic [31:0] data
    );
        int cycles;
        begin
            cycles = 0;
            @(negedge clock);
            s_apb_paddr = addr;
            s_apb_pwdata = data;
            s_apb_pwrite = 1'b1;
            s_apb_psel = 1'b1;
            s_apb_penable = 1'b0;

            @(negedge clock);
            s_apb_penable = 1'b1;

            while (s_apb_pready !== 1'b1 && cycles < 10) begin
                @(posedge clock);
                #1;
                cycles++;
            end

            if (cycles >= 10) begin
                $error("APB write timed out: addr=0x%0h data=0x%08h", addr, data);
                failures++;
            end

            if (s_apb_pslverr !== 1'b0) begin
                $error("APB write signaled slave error: addr=0x%0h data=0x%08h", addr, data);
                failures++;
            end

            @(posedge clock);
            @(negedge clock);
            s_apb_psel = 1'b0;
            s_apb_penable = 1'b0;
            s_apb_pwrite = 1'b0;
            s_apb_pwdata = 32'h0;
        end
    endtask

    task automatic apb_read(
        input logic [3:0] addr,
        output logic [31:0] data
    );
        int cycles;
        begin
            cycles = 0;
            @(negedge clock);
            s_apb_paddr = addr;
            s_apb_pwdata = 32'h0;
            s_apb_pwrite = 1'b0;
            s_apb_psel = 1'b1;
            s_apb_penable = 1'b0;

            @(negedge clock);
            s_apb_penable = 1'b1;

            while (s_apb_pready !== 1'b1 && cycles < 10) begin
                @(posedge clock);
                #1;
                cycles++;
            end

            if (cycles >= 10) begin
                $error("APB read timed out: addr=0x%0h", addr);
                failures++;
                data = 32'hx;
            end else begin
                data = s_apb_prdata;
            end

            if (s_apb_pslverr !== 1'b0) begin
                $error("APB read signaled slave error: addr=0x%0h", addr);
                failures++;
            end

            @(posedge clock);
            @(negedge clock);
            s_apb_psel = 1'b0;
            s_apb_penable = 1'b0;
        end
    endtask

    task automatic check_apb_read(
        input string name,
        input logic [3:0] addr,
        input logic [31:0] expected
    );
        logic [31:0] actual;
        begin
            apb_read(addr, actual);
            if (actual !== expected) begin
                $error("%s failed: expected=0x%08h actual=0x%08h", name, expected, actual);
                failures++;
            end
        end
    endtask

    task automatic ps2_drive_bit(input logic bit_value);
        begin
            ps2_data = bit_value;
            repeat (2) @(posedge clock);
            ps2_clock = 1'b0;
            repeat (4) @(posedge clock);
            ps2_clock = 1'b1;
            repeat (4) @(posedge clock);
        end
    endtask

    task automatic ps2_send_byte(input logic [7:0] scan_code);
        logic parity;
        begin
            parity = ~(^scan_code);

            ps2_drive_bit(1'b0);
            for (int i = 0; i < 8; i++)
                ps2_drive_bit(scan_code[i]);
            ps2_drive_bit(parity);
            ps2_drive_bit(1'b1);

            ps2_data = 1'b1;
            ps2_clock = 1'b1;
            repeat (4) @(posedge clock);
        end
    endtask

    initial begin
        reset_dut();

        check_apb_read("reset status", STATUS_CONTROL, 32'h0000_0000);
        check_apb_read("reset fifo count", FIFO_COUNT, 32'h0000_0000);
        check_apb_read("empty key event", KEY_EVENT, 32'h0000_0000);

        ps2_send_byte(8'h1C);
        check_apb_read("disabled keyboard ignores PS/2 events", FIFO_COUNT, 32'h0000_0000);

        apb_write(STATUS_CONTROL, STATUS_ENABLE);
        check_apb_read("enable bit is readable", STATUS_CONTROL, STATUS_ENABLE);

        ps2_send_byte(8'h1C);
        check_apb_read("one queued event count", FIFO_COUNT, 32'h0000_0001);
        check_apb_read("ready after event", STATUS_CONTROL, STATUS_ENABLE | STATUS_READY);
        check_apb_read("read pops key event", KEY_EVENT, EXPECTED_EVENT);
        check_apb_read("count after pop", FIFO_COUNT, 32'h0000_0000);
        check_apb_read("empty event after pop", KEY_EVENT, 32'h0000_0000);

        ps2_send_byte(8'h1D);
        ps2_send_byte(8'h1E);
        check_apb_read("two queued events count", FIFO_COUNT, 32'h0000_0002);
        apb_write(STATUS_CONTROL, STATUS_ENABLE | STATUS_CLEAR_FIFO);
        repeat (2) @(posedge clock);
        check_apb_read("clear FIFO count", FIFO_COUNT, 32'h0000_0000);
        check_apb_read("clear FIFO status", STATUS_CONTROL, STATUS_ENABLE);

        for (int i = 0; i < 16; i++)
            ps2_send_byte(8'h20 + i[7:0]);
        check_apb_read("full FIFO count", FIFO_COUNT, 32'h0000_0010);
        check_apb_read("full FIFO status", STATUS_CONTROL, STATUS_ENABLE | STATUS_READY | STATUS_FULL);
        ps2_send_byte(8'h30);
        check_apb_read("overflow status", STATUS_CONTROL, STATUS_ENABLE | STATUS_READY | STATUS_FULL | STATUS_OVERFLOW);
        check_apb_read("overflow preserves count", FIFO_COUNT, 32'h0000_0010);
        apb_write(STATUS_CONTROL, STATUS_ENABLE | STATUS_OVERFLOW);
        check_apb_read("clear overflow status", STATUS_CONTROL, STATUS_ENABLE | STATUS_READY | STATUS_FULL);

        if (failures == 0)
            $display("All keyboard tests passed.");
        else
            $error("%0d keyboard test(s) failed.", failures);

        if (finish_on_done)
            $finish;
    end

endmodule
