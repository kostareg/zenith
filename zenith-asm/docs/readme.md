# Zenith Assembler

This directory contains the C++ assembler for Zenith. It is organized as:

- `lib/zenith-asm`: the reusable emulator library and public headers.
- `apps/zenith-asm`: a native executable that links against the library.

## Native build

```bash
cmake --preset native-debug
cmake --build --preset native-debug
./build/native-debug/apps/zenith-asm/zenith-asm-app
```

## WebAssembly build

Install and activate Emscripten first so the `EMSCRIPTEN` environment variable points at the SDK, then run:

```bash
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
cp ./build/wasm-debug/zenith-asm.{js,wasm,d.ts} ../zenith-web/src/wasm/
```

The WebAssembly build emits `build/wasm-debug/zenith-asm.{js,d.ts}` and the paired `.wasm` binary. The generated module exports the embind `Emulator` class:

```ts
import createModule from "./zenith-asm.js";

const module = await createModule();
const assembler = new module.Assembler();

// todo
```
