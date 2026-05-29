`timescale 1ns / 1ps

package zenith_instructions;
    typedef enum logic [6:0] {
        INSTR_ADD  = 7'h00,
        INSTR_SUB  = 7'h01,
        INSTR_MUL  = 7'h02,
        INSTR_DIV  = 7'h03,
        INSTR_ADDI = 7'h04,
        INSTR_MULI = 7'h05,
        INSTR_DIVI = 7'h06,
        INSTR_AND  = 7'h07,
        INSTR_OR   = 7'h08,
        INSTR_XOR  = 7'h09,
        INSTR_L8   = 7'h0A,
        INSTR_L16  = 7'h0B,
        INSTR_L32  = 7'h0C,
        INSTR_L64  = 7'h0D,
        INSTR_S8   = 7'h0E,
        INSTR_S16  = 7'h0F,
        INSTR_S32  = 7'h10,
        INSTR_S64  = 7'h11,
        INSTR_BEQ  = 7'h14,
        INSTR_BNE  = 7'h15,
        INSTR_BGE  = 7'h16,
        INSTR_BLE  = 7'h17,
        INSTR_BGT  = 7'h18,
        INSTR_BLT  = 7'h19,
        INSTR_JAL  = 7'h1A,
        INSTR_JALR = 7'h1B,
        INSTR_NOT  = 7'h1C,
        INSTR_SLL  = 7'h1D,
        INSTR_SRL  = 7'h1E,
        INSTR_SLA  = 7'h1F,
        INSTR_SRA  = 7'h20
    } instruction_t;
endpackage
