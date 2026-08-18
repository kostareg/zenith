`timescale 1ns / 1ps

import zenith_instructions::*;

module control_flow_tb #(
    parameter bit finish_on_done = 1'b1
);

    reg clock = 0;
    always #5 clock = ~clock;

    logic [3:0] digit;
    logic [7:0] led_out;
    int failures = 0;

    main dut (
        .clock(clock),
        .digit(digit),
        .led_out(led_out)
    );

    function automatic logic [31:0] encode_register(
        input logic [6:0] opcode,
        input logic [4:0] field1,
        input logic [4:0] field2,
        input logic [4:0] field3
    );
        encode_register = {10'd0, field3, field2, field1, opcode};
    endfunction

    function automatic logic [31:0] encode_immediate(
        input logic [6:0] opcode,
        input logic [4:0] field1,
        input logic [4:0] field2,
        input int immediate
    );
        encode_immediate = {immediate[14:0], field2, field1, opcode};
    endfunction

    function automatic logic [31:0] encode_jump(
        input logic [6:0] opcode,
        input logic [4:0] rd,
        input int offset
    );
        encode_jump = {offset[19:0], rd, opcode};
    endfunction

    task automatic set_instruction(
        input int address,
        input logic [31:0] instruction
    );
        begin
            dut.mem.mem[address]     = instruction[7:0];
            dut.mem.mem[address + 1] = instruction[15:8];
            dut.mem.mem[address + 2] = instruction[23:16];
            dut.mem.mem[address + 3] = instruction[31:24];
        end
    endtask

    task automatic check_register(
        input string name,
        input int index,
        input logic [63:0] expected
    );
        begin
            if (dut.rf.registers[index] !== expected) begin
                $error(
                    "%s failed: r%0d expected=0x%016h actual=0x%016h",
                    name,
                    index,
                    expected,
                    dut.rf.registers[index]
                );
                failures++;
            end
        end
    endtask

    task automatic wait_for_fetch_pc(
        input logic [63:0] expected_pc,
        input int max_cycles
    );
        int cycles;
        begin
            cycles = 0;
            while ((dut.pc !== expected_pc || dut.state !== 3'b000) && cycles < max_cycles) begin
                @(posedge clock);
                #1;
                cycles++;
            end

            if (cycles >= max_cycles) begin
                $error(
                    "timed out waiting for FETCH at pc=0x%016h; actual pc=0x%016h state=%0d",
                    expected_pc,
                    dut.pc,
                    dut.state
                );
                failures++;
            end
        end
    endtask

    initial begin
        for (int i = 0; i < 4096; i++)
            dut.mem.mem[i] = 8'd0;
        for (int i = 0; i < 32; i++)
            dut.rf.registers[i] = 64'd0;

        set_instruction(0,   encode_immediate(INSTR_ADDI, 5'd1, 5'd0, 5));      // r1 = 5
        set_instruction(4,   encode_immediate(INSTR_ADDI, 5'd2, 5'd0, 5));      // r2 = 5
        set_instruction(8,   encode_immediate(INSTR_BEQ,  5'd1, 5'd2, 16));     // taken to 24
        set_instruction(12,  encode_immediate(INSTR_ADDI, 5'd3, 5'd0, 1));      // skipped
        set_instruction(16,  encode_immediate(INSTR_ADDI, 5'd3, 5'd0, 2));      // skipped
        set_instruction(20,  encode_immediate(INSTR_ADDI, 5'd3, 5'd0, 3));      // skipped
        set_instruction(24,  encode_immediate(INSTR_ADDI, 5'd3, 5'd0, 4));      // r3 = 4
        set_instruction(28,  encode_immediate(INSTR_BNE,  5'd1, 5'd2, 8));      // not taken
        set_instruction(32,  encode_immediate(INSTR_ADDI, 5'd4, 5'd0, 7));      // r4 = 7
        set_instruction(36,  encode_immediate(INSTR_ADDI, 5'd5, 5'd0, -1));     // r5 = -1
        set_instruction(40,  encode_immediate(INSTR_ADDI, 5'd6, 5'd0, 1));      // r6 = 1
        set_instruction(44,  encode_immediate(INSTR_BNE,  5'd5, 5'd6, 8));      // taken to 52
        set_instruction(48,  encode_immediate(INSTR_ADDI, 5'd7, 5'd0, 7));      // skipped
        set_instruction(52,  encode_immediate(INSTR_BLT,  5'd5, 5'd6, 8));      // taken to 60
        set_instruction(56,  encode_immediate(INSTR_ADDI, 5'd8, 5'd0, 8));      // skipped
        set_instruction(60,  encode_immediate(INSTR_BGE,  5'd6, 5'd5, 8));      // taken to 68
        set_instruction(64,  encode_immediate(INSTR_ADDI, 5'd9, 5'd0, 9));      // skipped
        set_instruction(68,  encode_immediate(INSTR_BGT,  5'd6, 5'd5, 8));      // taken to 76
        set_instruction(72,  encode_immediate(INSTR_ADDI, 5'd10, 5'd0, 10));    // skipped
        set_instruction(76,  encode_immediate(INSTR_BLE,  5'd5, 5'd6, 8));      // taken to 84
        set_instruction(80,  encode_immediate(INSTR_ADDI, 5'd12, 5'd0, 12));    // skipped
        set_instruction(84,  encode_jump(INSTR_JAL, 5'd11, 8));                 // r11 = 88, pc = 92
        set_instruction(88,  encode_immediate(INSTR_ADDI, 5'd13, 5'd0, 13));    // skipped
        set_instruction(92,  encode_immediate(INSTR_ADDI, 5'd14, 5'd0, 104));   // r14 = jalr base
        set_instruction(96,  encode_immediate(INSTR_JALR, 5'd15, 5'd14, 8));    // r15 = 100, pc = 112
        set_instruction(100, encode_immediate(INSTR_ADDI, 5'd16, 5'd0, 16));    // skipped
        set_instruction(104, encode_immediate(INSTR_ADDI, 5'd17, 5'd0, 17));    // skipped
        set_instruction(108, encode_immediate(INSTR_ADDI, 5'd18, 5'd0, 18));    // skipped
        set_instruction(112, encode_immediate(INSTR_ADDI, 5'd19, 5'd0, 19));    // r19 = 19
        set_instruction(116, encode_immediate(INSTR_BEQ,  5'd0, 5'd0, 0));      // self-loop

        wait_for_fetch_pc(64'd116, 200);
        repeat (4) begin
            @(posedge clock);
            #1;
        end

        check_register("beq taken target", 3, 64'd4);
        check_register("bne not taken fallthrough", 4, 64'd7);
        check_register("bne taken skipped", 7, 64'd0);
        check_register("blt signed taken skipped", 8, 64'd0);
        check_register("bge signed taken skipped", 9, 64'd0);
        check_register("bgt signed taken skipped", 10, 64'd0);
        check_register("ble signed taken skipped", 12, 64'd0);
        check_register("jal link", 11, 64'd88);
        check_register("jal skipped fallthrough", 13, 64'd0);
        check_register("jalr base", 14, 64'd104);
        check_register("jalr link", 15, 64'd100);
        check_register("jalr skipped target gap", 16, 64'd0);
        check_register("jalr skipped target gap", 17, 64'd0);
        check_register("jalr skipped target gap", 18, 64'd0);
        check_register("jalr target", 19, 64'd19);

        if (failures == 0)
            $display("All control flow tests passed.");
        else
            $error("%0d control flow test(s) failed.", failures);

        if (finish_on_done)
            $finish;
    end

endmodule
