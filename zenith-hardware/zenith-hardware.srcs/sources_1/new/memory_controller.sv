`timescale 1ns / 1ps

/**
 * @brief memory controller little-endian
 * @param clock clock
 * @param mem_r whether to read addr from memory
 * @param mem_w whether to write to addr in memory
 * @param mem_width access width: 00=8-bit, 01=16-bit, 10=32-bit, 11=64-bit
 * @param data_in data to write
 * @param data_out data read
 * @param mem_ready whether the requested access is complete
 */
module memory_controller(
    input logic clock,
    input logic mem_r,
    input logic mem_w,
    input logic [1:0] mem_width,
    input logic [63:0] mem_addr,
    input logic [63:0] mem_in,
    output logic [63:0] mem_out,
    output logic mem_ready
);
    // todo: this is fake 4kiB RAM
    logic [7:0] mem [0:4095];
    initial begin
        // addi z1, z0, 5
        mem[0] = 8'h84;
        mem[1] = 8'h00;
        mem[2] = 8'h0A;
        mem[3] = 8'h00;

        // addi z2, z0, 3
        mem[4] = 8'h04;
        mem[5] = 8'h01;
        mem[6] = 8'h06;
        mem[7] = 8'h00;

        // add z3, z1, z2
        mem[8]  = 8'h80;
        mem[9]  = 8'h11;
        mem[10] = 8'h04;
        mem[11] = 8'h00;
    end

    always@(posedge clock) begin
        if (mem_r) begin
            case (mem_width)
                2'b00: mem_out <= {56'd0, mem[mem_addr]};
                2'b01: mem_out <= {48'd0, mem[mem_addr+1], mem[mem_addr]};
                2'b10: mem_out <= {32'd0, mem[mem_addr+3], mem[mem_addr+2], mem[mem_addr+1], mem[mem_addr]};
                2'b11: mem_out <= {mem[mem_addr+7], mem[mem_addr+6], mem[mem_addr+5], mem[mem_addr+4],
                                    mem[mem_addr+3], mem[mem_addr+2], mem[mem_addr+1], mem[mem_addr]};
            endcase
            mem_ready <= 1'b1;
        end else if (mem_w) begin
            case (mem_width)
                2'b00: begin
                    mem[mem_addr] <= mem_in[7:0];
                end
                2'b01: begin
                    mem[mem_addr]   <= mem_in[7:0];
                    mem[mem_addr+1] <= mem_in[15:8];
                end
                2'b10: begin
                    mem[mem_addr]   <= mem_in[7:0];
                    mem[mem_addr+1] <= mem_in[15:8];
                    mem[mem_addr+2] <= mem_in[23:16];
                    mem[mem_addr+3] <= mem_in[31:24];
                end
                2'b11: begin
                    mem[mem_addr]   <= mem_in[7:0];
                    mem[mem_addr+1] <= mem_in[15:8];
                    mem[mem_addr+2] <= mem_in[23:16];
                    mem[mem_addr+3] <= mem_in[31:24];
                    mem[mem_addr+4] <= mem_in[39:32];
                    mem[mem_addr+5] <= mem_in[47:40];
                    mem[mem_addr+6] <= mem_in[55:48];
                    mem[mem_addr+7] <= mem_in[63:56];
                end
            endcase
            mem_ready <= 1'b1;
        end else begin
            mem_ready <= 1'b0;
        end
    end
endmodule
