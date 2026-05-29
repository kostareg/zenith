`timescale 1ns / 1ps

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