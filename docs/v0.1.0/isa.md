## Zenith Instruction Set Architecture v0.1.0

### Summary

Zenith is a 64-bit little-endian CPU coupled with a tile-based tensor accelerator. Instructions are fixed-width 32 bits, it uses a load/store architecture, and has byte-addressable memory. See below for details on registers, instructions, and traps.

### Registers

#### CPU Registers

Zenith CPU has 32 registers. They are almost all general-purpose, but have some conventions as described below. Each are one word (64 bits) and memory is little-endian. 

| zn      | name  | description                 |
|---------|-------|-----------------------------|
| z0      | zero  | always zero                 |
| z1      | ra    | return address              |
| z2      | sp    | stack pointer               |
| z3–z10  | a0–a7 | arguments / return values   |
| z11–z18 | t0–t7 | caller-saved                |
| z19–z26 | s0–s7 | callee-saved                |
| z27–z31 | k0–k4 | kernel trap / scratch       |

#### TPU Registers

The TPU has 32 registers for the tensor unit, each being 1024 bits. This can store, for example, an 8x8 16-bit tensor.

### Instructions

#### Arithmetic

| Instruction           | Description            |
|-----------------------|------------------------|
| add rd, rs1, rs2      | rd = rs1 + rs2         |
| sub rd, rs1, rs2      | rd = rs1 - rs2         |
| mul rd, rs1, rs2      | rd = rs1 * rs2         |
| div rd, rs1, rs2      | rd = rs1 / rs2[^1]     |
| addi rd, rs1, imm     | rd = rs1 + imm         |
| muli rd, rs1, imm     | rd = rs1 * imm         |
| divi rd, rs1, imm     | rd = rs1 / imm         |
| and rd, rs1, rs2      | bitwise and            |
| or rd, rs1, rs2       | bitwise or             |
| xor rd, rs1, rs2      | bitwise xor            |

[^1]: traps on divide by zero.

#### Memory

| Instruction        | Description                                  |
|--------------------|----------------------------------------------|
| l8  rd, imm(rs1)   | load 8-bit value, sign-extend to 64-bit      |
| l16 rd, imm(rs1)   | load 16-bit value, sign-extend to 64-bit     |
| l32 rd, imm(rs1)   | load 32-bit value, sign-extend to 64-bit     |
| l64 rd, imm(rs1)   | load 64-bit value                            |
| s8  rs2, imm(rs1)  | store lower 8 bits of rs2            |
| s16 rs2, imm(rs1)  | store lower 16 bits of rs2           |
| s32 rs2, imm(rs1)  | store lower 32 bits of rs2           |
| s64 rs2, imm(rs1)  | store full 64 bits of rs2            |
| tld qd, imm(rs1)   | load tensor            |
| tsd qs, imm(rs1)   | store tensor           |

#### Control

| Instruction           | Description                    |
|-----------------------|--------------------------------|
| beq rs1, rs2, off     | branch if equal                |
| bne rs1, rs2, off     | branch if not equal            |
| bge rs1, rs2, off     | branch if >=                   |
| ble rs1, rs2, off     | branch if <=                   |
| bgt rs1, rs2, off     | branch if >                    |
| blt rs1, rs2, off     | branch if <                    |
| jal rd, off           | rd = next pc; jump             |
| jalr rd, rs1, off     | rd = next pc; indirect jump    |

#### Tensor

| Instruction            | Description              |
|------------------------|--------------------------|
| tmatmul qd, qa, qb     | qd = qa * qb (matrix-wise)     |
| tmul qd, qa, qb        | qd = qa * qb (element-wise)    |
| tmac qd, qa, qb        | qd += qa * qb            |
| tadd qd, qa, qb        | qd = qa + qb             |
| tsub qd, qa, qb        | qd = qa - qb             |
| tmov qd, qs            | copy                     |
| tfill qd, imm          | fill                     |
| tscale qd, qs, imm     | shift / scale            |
| tmax rd, qs            | max reduction            |
| targmax rd, qs         | index of max element     |
| tsum rd, qs            | sum reduction            |
| tcast qd, rs           | broadcast scalar         |
| tsync                  | sync                     |

#### System

| Instruction | Description           |
|------------|-----------------------|
| ecall      | trap to kernel        |
| eret       | return from trap      |
| halt       | stop execution        |

### Modes

For my future reference:

* CPU boots in kernel mode
* Kernel launches programs in user mode
* User cannot access privileged resources directly
* Uses ECALL to request kernel services

### Traps

todo