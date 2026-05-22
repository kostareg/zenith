# Zenith Assembler

This directory contains the C++ assembler for Zenith. It is organized as:

- `lib/zenith-assembler`: the reusable assembler library and public headers.
- `apps/zenith-assembler`: a native executable that links against the library.

## Native build

```bash
cmake --preset native-debug
cmake --build --preset native-debug
./build/native-debug/apps/zenith-assembler/zenith-assembler-app
ctest --test-dir build/native-debug --output-on-failure
```

## WebAssembly build

Install and activate Emscripten first so the `EMSCRIPTEN` environment variable points at the SDK, then run:

```bash
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
cp ./build/wasm-debug/zenith-assembler.{js,wasm,d.ts} ../zenith-web/src/wasm/
```

The WebAssembly build emits `build/wasm-debug/zenith-assembler.{js,d.ts}` and the paired `.wasm` binary. The generated module exports the embind `Assembler` class:

```ts
import createModule from "./zenith-assembler.js";

const module = await createModule();
const assembler = new module.Assembler();

console.log(assembler.version());
```

## Syntax

```
# lines that start with # are comments
# start with .main
.main
  add r0, r0, r1
# define a label before a section to reference it later
  mylabel: add r1, r1, r1
  j mylabel # desugars to jal ...

# define exact data in data
.data
  label: .byte 100
```
