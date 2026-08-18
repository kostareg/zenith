`timescale 1ns / 1ps

import zenith_instructions::*;

typedef enum logic [2:0] {
    FETCH = 3'b000,
    FETCH_WAIT = 3'b001,
    DECODE = 3'b010,
    EXECUTE = 3'b011,
    MEMORY_WAIT = 3'b100
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
    logic [63:0] instruction_pc;
    logic [31:0] instruction;

    // register file
    logic rf_w;
    logic [4:0] reg_a, reg_b, reg_w;
    logic [63:0] data_w, data_a, data_b;
    logic signed [63:0] signed_data_a, signed_data_b;
    logic signed [14:0] imm15;
    logic signed [19:0] imm20;
    logic [63:0] imm15_extended;
    logic [63:0] imm20_extended;

    // memory controller
    logic mem_r, mem_w, mem_ready;
    logic [1:0] mem_width;
    logic [63:0] mem_addr, mem_in, mem_out;

    // alu
    logic [63:0] alu_a, alu_b, alu_out;
    alu_op_t alu_op;
    logic [1:0] alu_a_select;
    logic [1:0] alu_b_select;
    assign alu_a = alu_a_select[1] ? instruction_pc :
                   alu_a_select[0] ? pc : data_a;
    assign alu_b = alu_b_select[1] ? (alu_b_select[0] ? imm20_extended : 64'd4) :
                   alu_b_select[0] ? imm15_extended : data_b;
    assign led_out = alu_out; // todo: remove

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
        .mem_width(mem_width),
        .mem_addr(mem_addr),
        .mem_in(mem_in),
        .mem_out(mem_out),
        .mem_ready(mem_ready)
    );
    arithmetic_logic_unit alu (
        .alu_a(alu_a),
        .alu_b(alu_b),
        .alu_op(alu_op),
        .alu_out(alu_out)
    );

    assign signed_data_a = data_a;
    assign signed_data_b = data_b;
    assign imm15_extended = {{49{imm15[14]}}, imm15};
    assign imm20_extended = {{44{imm20[19]}}, imm20};

    always@(posedge clock) case (state)
        FETCH: begin
            rf_w <= 1'b0;
            mem_addr <= pc;
            mem_r <= 1'b1;
            mem_w <= 1'b0;
            mem_width <= 2'b10;
            alu_a_select <= 2'b01;
            alu_b_select <= 2'b10;
            alu_op <= ADD;
            state <= FETCH_WAIT;
        end

        FETCH_WAIT: begin
            if (mem_ready) begin
                instruction_pc <= pc;
                instruction <= mem_out[31:0];
                pc <= alu_out;
                mem_r <= 1'b0;
                state <= DECODE;
            end
        end

        DECODE: begin
            case (instruction[6:0])
                INSTR_ADD, INSTR_SUB, INSTR_MUL, INSTR_DIV, INSTR_AND, INSTR_OR,
                INSTR_XOR, INSTR_SLL, INSTR_SRL, INSTR_SLA, INSTR_SRA: begin
                    reg_w <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    reg_b <= instruction[21:17];
                    case (instruction[6:0])
                        INSTR_ADD: alu_op <= ADD;
                        INSTR_SUB: alu_op <= SUB;
                        INSTR_MUL: alu_op <= MUL;
                        INSTR_DIV: alu_op <= DIV;
                        INSTR_AND: alu_op <= BIT_AND;
                        INSTR_OR:  alu_op <= BIT_OR;
                        INSTR_XOR: alu_op <= BIT_XOR;
                        INSTR_SLL: alu_op <= SLL;
                        INSTR_SRL: alu_op <= SRL;
                        INSTR_SLA: alu_op <= SLA;
                        INSTR_SRA: alu_op <= SRA;
                        default:   alu_op <= ADD;
                    endcase
                    alu_a_select <= 2'b00;
                    alu_b_select <= 2'b00;
                end
                INSTR_NOT: begin
                    reg_w <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    alu_op <= BIT_NOT;
                    alu_a_select <= 2'b00;
                    alu_b_select <= 2'b00;
                end
                INSTR_ADDI, INSTR_MULI, INSTR_DIVI: begin
                    reg_w <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    imm15 <= instruction[31:17];
                    case (instruction[6:0])
                        INSTR_ADDI: alu_op <= ADD;
                        INSTR_MULI: alu_op <= MUL;
                        INSTR_DIVI: alu_op <= DIV;
                        default:    alu_op <= ADD;
                    endcase
                    alu_a_select <= 2'b00;
                    alu_b_select <= 2'b01;
                end
                INSTR_L8, INSTR_L16, INSTR_L32, INSTR_L64: begin
                    reg_w <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    imm15 <= instruction[31:17];
                    alu_op <= ADD;
                    alu_a_select <= 2'b00;
                    alu_b_select <= 2'b01;
                end
                INSTR_S8, INSTR_S16, INSTR_S32, INSTR_S64: begin
                    reg_b <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    imm15 <= instruction[31:17];
                    alu_op <= ADD;
                    alu_a_select <= 2'b00;
                    alu_b_select <= 2'b01;
                end
                INSTR_BEQ, INSTR_BNE, INSTR_BGE, INSTR_BLE, INSTR_BGT, INSTR_BLT: begin
                    reg_a <= instruction[11:7];
                    reg_b <= instruction[16:12];
                    imm15 <= instruction[31:17];
                    alu_op <= ADD;
                    alu_a_select <= 2'b10;
                    alu_b_select <= 2'b01;
                end
                INSTR_JAL: begin
                    reg_w <= instruction[11:7];
                    imm20 <= instruction[31:12];
                    alu_op <= ADD;
                    alu_a_select <= 2'b10;
                    alu_b_select <= 2'b11;
                end
                INSTR_JALR: begin
                    reg_w <= instruction[11:7];
                    reg_a <= instruction[16:12];
                    imm15 <= instruction[31:17];
                    alu_op <= ADD;
                    alu_a_select <= 2'b00;
                    alu_b_select <= 2'b01;
                end
            endcase
            state <= EXECUTE;
        end
        EXECUTE: begin
            case (instruction[6:0])
                INSTR_ADD, INSTR_SUB, INSTR_MUL, INSTR_ADDI, INSTR_MULI,
                INSTR_DIV, INSTR_DIVI, INSTR_AND, INSTR_OR, INSTR_XOR, INSTR_NOT,
                INSTR_SLL, INSTR_SRL, INSTR_SLA, INSTR_SRA: begin
                    data_w <= alu_out;
                    rf_w <= 1'b1;
                    state <= FETCH;
                end
                INSTR_L8, INSTR_L16, INSTR_L32, INSTR_L64: begin
                    mem_addr <= alu_out;
                    mem_r <= 1'b1;
                    mem_w <= 1'b0;
                    case (instruction[6:0])
                        INSTR_L8:  mem_width <= 2'b00;
                        INSTR_L16: mem_width <= 2'b01;
                        INSTR_L32: mem_width <= 2'b10;
                        default:   mem_width <= 2'b11;
                    endcase
                    state <= MEMORY_WAIT;
                end
                INSTR_S8, INSTR_S16, INSTR_S32, INSTR_S64: begin
                    mem_addr <= alu_out;
                    mem_in <= data_b;
                    mem_r <= 1'b0;
                    mem_w <= 1'b1;
                    case (instruction[6:0])
                        INSTR_S8:  mem_width <= 2'b00;
                        INSTR_S16: mem_width <= 2'b01;
                        INSTR_S32: mem_width <= 2'b10;
                        default:   mem_width <= 2'b11;
                    endcase
                    state <= MEMORY_WAIT;
                end
                INSTR_BEQ: begin
                    if (data_a == data_b)
                        pc <= alu_out;
                    state <= FETCH;
                end
                INSTR_BNE: begin
                    if (data_a != data_b)
                        pc <= alu_out;
                    state <= FETCH;
                end
                INSTR_BGE: begin
                    if (signed_data_a >= signed_data_b)
                        pc <= alu_out;
                    state <= FETCH;
                end
                INSTR_BLE: begin
                    if (signed_data_a <= signed_data_b)
                        pc <= alu_out;
                    state <= FETCH;
                end
                INSTR_BGT: begin
                    if (signed_data_a > signed_data_b)
                        pc <= alu_out;
                    state <= FETCH;
                end
                INSTR_BLT: begin
                    if (signed_data_a < signed_data_b)
                        pc <= alu_out;
                    state <= FETCH;
                end
                INSTR_JAL, INSTR_JALR: begin
                    data_w <= pc;
                    rf_w <= 1'b1;
                    pc <= alu_out;
                    state <= FETCH;
                end
                default: begin
                    state <= FETCH;
                end
            endcase
        end
        MEMORY_WAIT: begin
            mem_r <= 1'b0;
            mem_w <= 1'b0;
            if (mem_ready) begin
                case (instruction[6:0])
                    INSTR_L8: begin
                        data_w <= {{56{mem_out[7]}}, mem_out[7:0]};
                        rf_w <= 1'b1;
                    end
                    INSTR_L16: begin
                        data_w <= {{48{mem_out[15]}}, mem_out[15:0]};
                        rf_w <= 1'b1;
                    end
                    INSTR_L32: begin
                        data_w <= {{32{mem_out[31]}}, mem_out[31:0]};
                        rf_w <= 1'b1;
                    end
                    INSTR_L64: begin
                        data_w <= mem_out;
                        rf_w <= 1'b1;
                    end
                    default: begin
                        rf_w <= 1'b0;
                    end
                endcase
                state <= FETCH;
            end
        end
        default: state <= FETCH;
    endcase
endmodule
