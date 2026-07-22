# Zenith

Zenith is a 64-bit CPU and tensor accelerator. For the specification, see [docs/v0.1.0.md](https://github.com/kostareg/zenith/blob/main/docs/v0.1.0.md).

## Background

Zenith is a broad project that attempts to cover every part of the stack of a computer. Its end goal is to run a small
language model like quantized [SmolLM](https://github.com/huggingface/smollm) all by itself. In order to achieve this,
Zenith implements the following:

* `zenith-pcba` for the printed circuit board for Zenith. This includes a board with the relevant I/O, memory, and a
Xilinx Kintex-7 FPGA in order to run digital logic.
* `zenith-hardware` for the digital logic of the processor. This implementation is a 64-bit Von Neumann computer with
an extension of instructions for tensor-based operations such as matmul, argmax, etc. This extension is effectively a
[tile-based](https://docs.nvidia.com/deeplearning/performance/dl-performance-matrix-multiplication/index.html) 
[NPU](https://en.wikipedia.org/wiki/Neural_processing_unit) that will enable us to run small language models as is the
goal. Outside of this extension, we have regular CPU instructions for math, control flow, system calls, etc. and memory
mappings for I/O such as a VGA framebuffer, PS/2 keyboard and mouse, and SD card.
* `zenith-emulator` for a native emulator of the zenith hardware. It is helpful to run an emulator for this hardware
for debugging and research purposes. This emulator is implemented in C++ and runs natively, but can also be compiled to
WebAssembly (see zenith-web bullet).
* `zenith-assembler` for a basic assembly language. This simple assembler is able to transpile assembly language
instructions to the numerical instruction values themselves. It is also able to construct executable format files if
needed.
* `zenith-compiler` for a C-like programming language. This is a simple programming language that looks like C and
compiles down to Zenith Assembly. Enables easier programming of user space applications for this project.
* `zenith-kernel` and `zenith-libc` for a kernel and standard library. To note, the kernel is extremely light, as it is
out of the scope of the current revision. I'd like to explore expanding its functionality in the future. The standard
library is written for userspace applications and has basic functionality like printing, string comparison, etc. Both
of these are written in Zenith C.
* `zenith-web` for packaging the emulator in WebAssembly and running it on the web. It provides a user interface for writing
Zenith C, Assembly, or instructions, and includes some I/O out of the box. This has been especially useful for
debugging userspace applications.

## Progress

This table is an estimation of the features remaining until the first "release-ready" version of Zenith. N/A specifies not applicable.

|                            | Defined | Emulator (wasm) | Emulator (native) | RTL (simulation) | RTL (native) | PCB | Other |
| -------------------------- | ------- | --------------- | ----------------- | ---------------- | ------------ | --- | ----- |
| Arithmetic Instruction Set | ✅       | ✅               | ✅                 | ✅                | ✅            | N/A | N/A   |
| Memory Instruction Set     | ✅       | ✅               | ✅                 | ✅                | ✅            | N/A | N/A   |
| Control Instruction Set    | ✅       | ✅               | ✅                 | ✅                | ✅            | N/A | N/A   |
| Tensor Instruction Set     | ✅       | ❌               | ❌                 | ❌                | ❌            | N/A | N/A   |
| System Instruction Set     | ✅       | ❌               | ❌                 | ❌                | ❌            | N/A | N/A   |
| Executable Format          | ✅       | N/A             | N/A               | N/A              | N/A          | N/A | ❌     |
| Filesystem Format          | ✅       | N/A             | N/A               | N/A              | N/A          | N/A | ❌     |
| System Calls               | ✅       | N/A             | N/A               | N/A              | N/A          | N/A | ❌     |
| MMIO                       | -       | -               | -                 | -                | -            | -   | -     |
| ► Framebuffer (VGA)        | ✅       | ✅               | ✅                 | ✅                | ✅            | ✅   | N/A   |
| ► Keyboard (PS/2)          | ✅       | ✅               | ✅                 | ✅                | ✅            | ✅   | N/A   |
| ► Mouse (PS/2)             | ✅       | ❌               | ❌                 | ❌                | ❌            | ✅   | N/A   |
| ► Storage (SD)             | ✅       | ❌               | ❌                 | ❌                | ❌            | ✅   | N/A   |
| Assembly Language          | ✅       | N/A             | N/A               | N/A              | N/A          | N/A | ✅     |
| C-like Language            | ✅       | N/A             | N/A               | N/A              | N/A          | N/A | ✅     |
| Boot                       | N/A     | N/A             | N/A               | N/A              | N/A          | N/A | ❌     |
| Standard Library           | N/A     | N/A             | N/A               | N/A              | N/A          | N/A | ✅     |
| Kernel                     | N/A     | N/A             | N/A               | N/A              | N/A          | N/A | ❌     |

## Usage of Large Language Models

This is primarily a learning project for myself. I don't have a hard deadline nor a concrete set of features I need
before a set date. I'm mainly working on this to get a better understanding of what our computers and ML accelerators
do on a day-to-day basis - therefore, I use large language models very modestly here, mainly for research, finding PCBA
parts, and _not_ for writing code[^1]. Ask me about how I use language models in my work experience for more information.

[^1]: except for `zenith-web`. the frontend development component is outside of the scope of this project and I've
found it much easier to prompt Claude to make a nice design, based on some frameworks that I like to use like React and
shadcn.

## Development Dependencies

* Cmake
* Ninja
* Bun
* emsdk
* PeakRDL regblock (optionally: markdown and html)

These are all installed in the Nix shell, accessible with `nix develop`.

For hardware development, install
[Vivado Standard Edition](https://www.amd.com/en/products/software/adaptive-socs-and-fpgas/vivado.html). For PCB/A
development, see [KiCad](https://www.kicad.org/).

## References

[1] J. L. Hennessy and D. A. Patterson, Computer Organization And Design The Hardware/Software Interface. Cambridge, Ma Morgan Kaufman Publishers, 2018.

[2] J. L. Hennessy and D. A. Patterson, Computer Architecture: a Quantitative Approach. Cambridge, Ma: Morgan Kaufmann, 2019.
