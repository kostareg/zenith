`timescale 1ns / 1ps

module tensor_accelerator();

endmodule

/*
 * @brief processing element for polynomial evaluation
 * @param clock  clock
 * @param reset  active low reset
 * @param a_in   input A
 * @param b_in   input B
 * @param a_out  output A
 * @param b_out  output B
 * @param clear  clear all values
 * @param enable run the processing unit
 * @param acc    accumulator output
 */
module pe #(
    parameter DW = 16,
    parameter AW = 40
) (
    input  logic             clock,
    input  logic             reset,

    input  logic [DW-1:0]    a_in,
    input  logic [DW-1:0]    b_in,

    output logic [DW-1:0]    a_out,
    output logic [DW-1:0]    b_out,

    input  logic             clear,
    input  logic             enable,

    output logic [AW-1:0]    acc
);

    always_ff @(posedge clock) begin
        if (~reset) begin
            a_out <= '0;
            b_out <= '0;
            acc   <= '0;
        end else begin
            a_out <= a_in;
            b_out <= b_in;

            if (clear)
                acc <= '0;
            else if (enable)
                acc <= acc + a_in * b_in;
        end
    end

endmodule