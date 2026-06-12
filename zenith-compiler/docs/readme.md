# Zenith Compiler

This directory contains the C++ compiler for Zenith. It is organized as:

- `lib/zenith-compiler`: the reusable assembler library and public headers.
- `apps/zenith-compiler`: a native executable that links against the library.

## Native build

```bash
cmake --preset native-debug
cmake --build --preset native-debug
./build/native-debug/apps/zenith-compiler/zenith-compiler-app
ctest --test-dir build/native-debug --output-on-failure
```

## WebAssembly build

Install and activate Emscripten first so the `EMSCRIPTEN` environment variable points at the SDK, then run:

```bash
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
cp ./build/wasm-debug/zenith-compiler.{js,wasm,d.ts} ../zenith-web/src/wasm/
```

The WebAssembly build emits `build/wasm-debug/zenith-compiler.{js,d.ts}` and the paired `.wasm` binary. The generated module exports the embind `Compiler` class:

```ts
import createModule from "./zenith-compiler.js";

const module = await createModule();
const compiler = new module.Compiler();

console.log(compiler.version());
```

## Syntax

Syntax for Zenith C is based on the following elements and changes to the [C99 standard](https://en.wikipedia.org/wiki/C99):

* Function definition syntax, parameters, return types
* Block scoped local variables, global variables, static globals and functions
* Integer types (`char`, `short`, `int`, `long`, `int8_t`, `int16_t`, `int32_t`, `int64_t`, `signed/unsigned`, `bool`)
* Pointers, address-of operator, dereferencing, pointer arithmetic
* Arrays
* String literals
* Struct
* Enum
* `if`, `else`, `while`, `for`, `break`, `continue`, `return`, `switch`/`case`/`default`
* Arithmetic, bitwise operations, comparisons, logical operations, assignment operations
* `#include` and `#define` preprocessing
* Ternary operators
* Single- and multi-line comments
* True and false keywords
