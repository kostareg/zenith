`timescale 1ns / 1ps

typedef enum logic [1:0] {
    FETCH = 2'b00,
    DECODE = 2'b01,
    EXECUTE = 2'b10,
    CYCLE = 2'b11
} state_t;

module main (
    input logic CLOCK,
    output logic [3:0] DIG,
    output logic [7:0] LED_out
);

    state_t state;
    reg pc = 0;
    
    always@(posedge CLOCK) begin
        
        case (state)

            FETCH:   state <= DECODE;
            DECODE:  state <= EXECUTE;
            EXECUTE: state <= CYCLE;
            CYCLE:   state <= FETCH;
            default: state <= FETCH;
        
        endcase
        
    end

endmodule