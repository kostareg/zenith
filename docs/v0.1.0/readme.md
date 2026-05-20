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

Arithmetic overflow is undefined behaviour.

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

Branch offsets are signed relative immediate values. Regular execution advances the program counter by 4 bytes (32
bits).

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

#### Instruction Encoding

All Zenith instructions are encoded as 32-bit little-endian words. The low 7 bits select the opcode.

| Bits  | Name    | Description                           |
|-------|---------|---------------------------------------|
| 0–6   | opcode  | instruction opcode                    |
| 7–11  | field1  | first register field                  |
| 12–16 | field2  | second register field                 |
| 17–21 | field3  | third register field                  |
| 22–31 | unused  | reserved                              |

Register-register instructions use `field1` as `rd`, `field2` as `rs1`, and `field3` as `rs2`.

| Bits  | Name   |
|-------|--------|
| 0–6   | opcode |
| 7–11  | rd     |
| 12–16 | rs1    |
| 17–21 | rs2    |
| 22–31 | unused |

Register-immediate, load, store, branch, and `jalr` instructions use a 15-bit signed immediate in bits 17–31.

| Bits  | Name           |
|-------|----------------|
| 0–6   | opcode         |
| 7–11  | rd/rs1/rs2     |
| 12–16 | rs1/rs2        |
| 17–31 | imm15 / off15  |

For arithmetic-immediate and load instructions, `field1` is `rd` and `field2` is `rs1`. For store instructions,
`field1` is `rs2` and `field2` is `rs1`. For branch instructions, `field1` is `rs1`, `field2` is `rs2`, and the
signed offset is relative to the current program counter. For `jalr`, `field1` is `rd`, `field2` is `rs1`, and the
signed offset is relative to `rs1`.

`jal` uses a 20-bit signed offset in bits 12–31.

| Bits  | Name  |
|-------|-------|
| 0–6   | opcode |
| 7–11  | rd    |
| 12–31 | off20 |

Assigned opcodes:

| Opcode | Instruction |
|--------|-------------|
| 0x00   | add         |
| 0x01   | sub         |
| 0x02   | mul         |
| 0x03   | div         |
| 0x04   | addi        |
| 0x05   | muli        |
| 0x06   | divi        |
| 0x07   | and         |
| 0x08   | or          |
| 0x09   | xor         |
| 0x0A   | l8          |
| 0x0B   | l16         |
| 0x0C   | l32         |
| 0x0D   | l64         |
| 0x0E   | s8          |
| 0x0F   | s16         |
| 0x10   | s32         |
| 0x11   | s64         |
| 0x14   | beq         |
| 0x15   | bne         |
| 0x16   | bge         |
| 0x17   | ble         |
| 0x18   | bgt         |
| 0x19   | blt         |
| 0x1A   | jal         |
| 0x1B   | jalr        |

Tensor and system opcode assignments are reserved for future revisions.

### Modes

For my future reference:

* CPU boots in kernel mode
* Kernel launches programs in user mode
* User cannot access privileged resources directly
* Uses ECALL to request kernel services

### Zelf Executable Format

Zelf is the Zenith executable format. It is a small little-endian binary format with a fixed header, one constant data
section, and one code section.

#### Header

| Offset | Size | Field       | Description                                |
|--------|------|-------------|--------------------------------------------|
| 0      | 4    | magic       | ASCII bytes `ZELF`                         |
| 4      | 1    | name_len    | program name length, 0 through 8           |
| 5      | 3    | reserved    | must be zero                               |
| 8      | 8    | name        | program name, zero-padded ASCII            |
| 16     | 8    | data_size   | constant data section length, bytes        |
| 24     | 8    | code_size   | code section length, bytes                 |
| 32     | 8    | entry_off   | entry point offset within the code section |

The header is 40 bytes. `data_size`, `code_size`, and `entry_off` are unsigned 64-bit little-endian values.

The `name` field is a short executable name stored directly in the header. It is not the filesystem path. If `name_len`
is less than 8, all remaining bytes in `name` must be zero.

#### Sections

The constant data section starts immediately after the header. The code section starts immediately after the data
section.

```text
+------------------+
| Zelf header      |
+------------------+
| constant data    |
+------------------+
| code             |
+------------------+
```

The code section contains raw 32-bit Zenith instructions. `code_size` must be a multiple of 4. `entry_off` must be a
multiple of 4 and must be less than `code_size`.

When a Zelf executable is loaded, the kernel copies the constant data section into read-only memory, copies the code
section into executable memory, and starts the child at `code_base + entry_off`. The exact load addresses are chosen by
the kernel.

### System Calls

System calls are requested with `ecall`. The syscall number is passed in `a7`. Arguments are passed in `a0` through
`a5`. Return values are passed in `a0` and `a1`.

`ecall` traps to the kernel. For system calls, the saved exception PC points to the instruction after `ecall`, so a
successful return resumes normal execution at the next instruction.

#### SYS_RUN

`SYS_RUN` loads a Zelf executable from the filesystem, runs it as a child program, and returns to the caller when that
program stops.

| Register | Input                        |
|----------|------------------------------|
| a7       | syscall number: `1`          |
| a0       | path address                 |
| a1       | path length, bytes           |
| a2       | argument block address       |
| a3       | argument block length, bytes |

| Register | Output                        |
|----------|-------------------------------|
| a0       | child exit code or error code |
| a1       | child trap cause              |

The path is an opaque byte string in the caller's memory. It is not null-terminated. If `a1` is zero, the syscall
fails. The kernel copies the path before reading from the filesystem.

The file at the requested path must be a valid Zelf executable. The kernel verifies the Zelf magic number before
starting the child.

The argument block is an opaque byte range in the caller's memory. If `a3` is zero, `a2` is ignored and no arguments
are passed. If `a3` is nonzero, the kernel copies `a3` bytes starting at `a2` before the child starts. The child
receives the copied argument block at an address chosen by the kernel.

`SYS_RUN` is blocking. The caller is suspended while the child runs. When the child stops, the kernel restores the
caller and resumes after the original `ecall`.

The child starts with:

* `pc` set to the child program entry point
* `sp` set to the top of a fresh child stack
* `a0` set to the child argument block address, or zero if no arguments were passed
* `a1` set to the child argument block length
* `a2` set to the constant data section address
* `a3` set to the constant data section length
* all other general-purpose registers set to zero

The child exits normally by executing `halt`. On normal exit, the child's final `a0` value becomes the caller's return
value in `a0`, and the caller's `a1` is set to zero.

If `SYS_RUN` fails before the child starts, the caller resumes immediately with a negative error code in `a0` and zero
in `a1`.

| Error | Meaning                         |
|-------|---------------------------------|
| -1    | invalid path address or length  |
| -2    | filesystem entry not found      |
| -3    | filesystem entry is not a file  |
| -4    | invalid Zelf executable         |
| -5    | invalid argument block address  |
| -6    | argument block is too large     |
| -7    | insufficient memory             |
| -8    | child program could not start   |

If the child traps instead of exiting normally, the caller resumes with `a0` set to `-9` and `a1` set to the child trap
cause. Trap cause values are defined by the trap specification.

Unless otherwise specified by the kernel ABI, `SYS_RUN` preserves the caller's registers except for `a0`, `a1`, and
kernel scratch registers.
