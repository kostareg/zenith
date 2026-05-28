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

    register_file rf();
    
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

/**
 * @brief the register file stores and provides access to all registers
 * @param clock  clock
 * @param rf_w   whether we should write data_w to reg_w
 * @param reg_a  register a select
 * @param reg_b  register b select
 * @param reg_w  register w select
 * @param data_w data to write to reg_w
 * @param data_a register value of a
 * @param data_b register value of b
 */
module register_file(
    input logic clock,
    input logic rf_w,
    input logic [4:0] reg_a,
    input logic [4:0] reg_b,
    input logic [4:0] reg_w,
    input logic [63:0] data_w,
    output logic [63:0] data_a,
    output logic [63:0] data_b
);
    logic [63:0] registers [0:31];

    initial registers[0] = 64'd0;

    assign data_a = registers[reg_a];
    assign data_b = registers[reg_b];

    always@(posedge clock) begin
        if (rf_w && reg_w != 5'd0) registers[reg_w] <= data_w;
    end
endmodule