`timescale 1ns / 1ps

typedef enum logic [2:0] {
    FETCH = 3'b000,
    FETCH_WAIT = 3'b001,
    DECODE = 3'b010,
    EXECUTE = 3'b011,
    CYCLE = 3'b100
} state_t;

/**
 * @brief top level processor
 * @param clock   clock
 * @param digit   selected digit to write to
 * @param led_out selected 7-segment value
 */
module main (
    input logic clock,
    output logic [3:0] digit,
    output logic [7:0] led_out
);
    logic [63:0] pc = 0;
    logic [31:0] instruction;

    // register file
    logic rf_w;
    logic [4:0] reg_a, reg_b, reg_w;
    logic [63:0] data_w, data_a, data_b;

    // memory controller
    logic mem_r, mem_w, mem_ready;
    logic [63:0] mem_addr, mem_in, mem_out;

    state_t state;
    register_file rf (
        .clock(clock),
        .rf_w(rf_w),
        .reg_a(reg_a),
        .reg_b(reg_b),
        .reg_w(reg_w),
        .data_w(data_w),
        .data_a(data_a),
        .data_b(data_b)
    );
    memory_controller mem (
        .clock(clock),
        .mem_r(mem_r),
        .mem_w(mem_w),
        .mem_addr(mem_addr),
        .mem_in(mem_in),
        .mem_out(mem_out),
        .mem_ready(mem_ready)
    );

    always@(posedge clock) case (state)
        FETCH: begin
            mem_addr <= pc;
            mem_r <= 1'b1;
            state <= FETCH_WAIT;
        end

        FETCH_WAIT: begin
            if (mem_ready) begin
                instruction <= mem_out[31:0];
                // todo: incorporate ALU
                pc <= pc + 4;
                mem_r <= 1'b0;
                state <= DECODE;
            end
        end

        DECODE:  state <= EXECUTE;
        EXECUTE: state <= CYCLE;
        CYCLE:   state <= FETCH;
        default: state <= FETCH;
    endcase
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

    always@(posedge clock)
        if (rf_w && reg_w != 5'd0)
            registers[reg_w] <= data_w;
endmodule

/**
 * @brief memory controller little-endian
 * @param clock clock
 * @param mem_r whether to read addr from memory
 * @param mem_w whether to write to addr in memory
 * @param data_in data to write
 * @param data_out data read
 * @param whether we are ready since the last mem_r
 */
module memory_controller(
    input logic clock,
    input logic mem_r,
    input logic mem_w,
    input logic [63:0] mem_addr,
    input logic [63:0] mem_in,
    output logic [63:0] mem_out,
    output logic mem_ready
);
    // todo: this is fake 4kiB RAM
    logic [7:0] mem [0:4095];
    initial mem[0] = 66;
    
    always@(posedge clock) begin
        if (mem_r) begin
            mem_out <= {mem[mem_addr+7], mem[mem_addr+6], mem[mem_addr+5], mem[mem_addr+4],
                        mem[mem_addr+3], mem[mem_addr+2], mem[mem_addr+1], mem[mem_addr]};
            mem_ready <= 1'b1;
        end else mem_ready <= 1'b0;

        if (mem_w) begin
            mem[mem_addr]   <= mem_in[7:0];
            mem[mem_addr+1] <= mem_in[15:8];
            mem[mem_addr+2] <= mem_in[23:16];
            mem[mem_addr+3] <= mem_in[31:24];
            mem[mem_addr+4] <= mem_in[39:32];
            mem[mem_addr+5] <= mem_in[47:40];
            mem[mem_addr+6] <= mem_in[55:48];
            mem[mem_addr+7] <= mem_in[63:56];
        end
    end
endmodule