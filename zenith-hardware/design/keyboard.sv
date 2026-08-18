import keyboard_mmio_pkg::*;

typedef struct packed {
    logic [15:0] key_code;
    logic        pressed;
    logic        repeat_;
} key_event_t;

module keyboard (
    input logic clock,
    input logic reset,

    // PS/2 lines
    input logic ps2_clock,
    input logic ps2_data,

    // MMIO
    input logic s_apb_psel,
    input logic s_apb_penable,
    input logic s_apb_pwrite,
    input logic [3:0] s_apb_paddr,
    input logic [31:0] s_apb_pwdata,
    output logic s_apb_pready,
    output logic [31:0] s_apb_prdata,
    output logic s_apb_pslverr
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

    logic has_new_event;
    key_event_t new_event;

    // read the PS2 data, store in FIFO
    always@(posedge clock) begin
        has_new_event <= 1'b0;
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

                // encode the new event
                has_new_event <= 1'b1;
                // todo: actually decode this
                new_event <= '{key_code: 16'd5, pressed: 1'b1, repeat_: 1'b0};
            end
            // otherwise shift to the left and add the current bit
            else begin
                ps2_current <= {ps2_bit, ps2_current[7:1]};
                ps2_parity <= ps2_parity + ps2_bit;
                ps2_counter <= ps2_counter + 1;
            end
        end else if (ps2_bit == 0) ps2_on <= 1'b1;
    end

    keyboard_mmio__in_t hwif_in;
    keyboard_mmio__out_t hwif_out;

    keyboard_mmio regs (
        .clk(clock),
        .rst(reset),
        .s_apb_psel,
        .s_apb_penable,
        .s_apb_pwrite,
        .s_apb_paddr,
        .s_apb_pwdata,
        .s_apb_pready,
        .s_apb_prdata,
        .s_apb_pslverr,
        .hwif_in,
        .hwif_out
    );

    key_event_t fifo [0:15];
    logic fifo_pop, fifo_push, fifo_empty, fifo_full, overflow_set;
    logic [4:0] fifo_count;
    assign fifo_empty = fifo_count == 5'd0;
    assign fifo_full = fifo_count == 5'd16;

    assign hwif_in.status_control.full.next = fifo_full;
    assign hwif_in.status_control.ready.next = ~fifo_empty;
    assign hwif_in.fifo_count.count.next = fifo_count;

    assign hwif_in.key_event.key_code.next = fifo_empty ? 16'h0 : fifo[0].key_code;
    assign hwif_in.key_event.pressed.next = fifo_empty ? 1'b0 : fifo[0].pressed;
    assign hwif_in.key_event.repeat_.next = fifo_empty ? 1'b0 : fifo[0].repeat_;

    assign hwif_in.status_control.overflow.next = hwif_out.status_control.overflow.value | overflow_set;

    // detect pops from APB
    // todo: is there a more SystemRDL-style way to do this?
    logic key_event_read;
    assign key_event_read =
        s_apb_psel && s_apb_penable && s_apb_pready &&
        !s_apb_pwrite && (s_apb_paddr[3:2] == 2'b01); // offset 0x4
    assign fifo_pop = key_event_read & ~fifo_empty;
    assign fifo_push = has_new_event & hwif_out.status_control.enable.value;

    logic overflow_clear;
    assign overflow_clear =
        s_apb_psel && s_apb_penable && s_apb_pready &&
        s_apb_pwrite && (s_apb_paddr[3:2] == 2'b00) && s_apb_pwdata[2];

    always@(posedge clock) begin
        if (reset) begin
            fifo <= '{default: '0};
            fifo_count <= 0;
            overflow_set <= 1'b0;
        end else begin
            if (overflow_clear)
                overflow_set <= 1'b0;

            if (hwif_out.status_control.clear_fifo.value) begin
                fifo <= '{default: '0};
                fifo_count <= 0;
            end else begin
                if (fifo_pop) begin
                    fifo[0:14] <= fifo[1:15];
                    fifo[15] <= '0;
                end
                if (fifo_push)
                    if (fifo_full && !fifo_pop) overflow_set <= 1'b1;
                    else if (fifo_pop) fifo[fifo_count - 1] <= new_event;
                    else fifo[fifo_count] <= new_event;

                if (fifo_push & ~fifo_pop & ~fifo_full) fifo_count <= fifo_count + 1;
                else if (~fifo_push & fifo_pop) fifo_count <= fifo_count - 1;
            end
        end
    end
endmodule
