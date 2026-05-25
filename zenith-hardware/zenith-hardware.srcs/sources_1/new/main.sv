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
    reg [63:0] pc = 0;
    reg [31:0] instruction;
    
    // todo: this is fake 4kiB RAM
    logic [7:0] mem [0:4095];
    
    initial begin
        mem[0] = 66;
    end
    
    always@(posedge CLOCK) begin
        
        case (state)

            FETCH: begin
            
                // we want to get the current pc from memory and place it in the instruction register
                instruction <= {mem[pc+7], mem[pc+6], mem[pc+5], mem[pc+4],
                                mem[pc+3], mem[pc+2], mem[pc+1], mem[pc]};
                
                pc <= pc + 4;
                state <= DECODE;
            end
            DECODE:  state <= EXECUTE;
            EXECUTE: state <= CYCLE;
            CYCLE:   state <= FETCH;
            default: state <= FETCH;
        
        endcase
        
    end

endmodule