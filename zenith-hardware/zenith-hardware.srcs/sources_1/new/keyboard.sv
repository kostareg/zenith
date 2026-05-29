module keyboard (
    input logic clock,
    input logic reset,

    // PS/2 lines
    input logic ps2_clock,
    input logic ps2_data
);
    import keyboard_mmio_pkg::*;

    keyboard_mmio regs (
        .clk(clock),
        .rst(reset)
    );

    // sync the ps2 clock
    logic [2:0] ps2_clk_sync;
    logic [2:0] ps2_data_sync;

    always_ff @(posedge clock) begin
        ps2_clk_sync  <= {ps2_clk_sync[1:0], ps2_clock};
        ps2_data_sync <= {ps2_data_sync[1:0], ps2_data};
    end

    logic ps2_falling_edge;
    assign ps2_falling_edge = ps2_clk_sync[2:1] == 2'b10;

    logic ps2_bit;
    assign ps2_bit = ps2_data_sync[2];

    logic ps2_on = 1'b0;
    logic [3:0] ps2_counter = 4'b0000;
    logic [7:0] ps2_current = 8'b00000000;
    logic [3:0] ps2_parity = 4'b0000;
    logic ps2_error = 1'b0;

    // read the PS2 data, store in FIFO
    always@(posedge clock) begin
        if (ps2_falling_edge) if (ps2_on) begin
            // we've reached the parity bit, perform an odd parity check
            if (ps2_counter == 4'd8) begin
                ps2_error <= ps2_parity[0] == ps2_bit;
                ps2_counter <= ps2_counter + 1;
            end
            // we've reached the STOP bit - ps2_data should be 1 here
            else if (ps2_counter == 4'd9) begin
                ps2_error <= ps2_error | ~ps2_bit;
                ps2_counter <= 1'b0;
                ps2_on <= 1'b0;
                ps2_parity <= 4'b0000;
            end
            // otherwise shift to the left and add the current bit
            else begin
                ps2_current <= {ps2_bit, ps2_current[7:1]};
                ps2_parity <= ps2_parity + ps2_bit;
                ps2_counter <= ps2_counter + 1;
            end
        end else if (ps2_bit == 0) ps2_on <= 1'b1;
    end
endmodule