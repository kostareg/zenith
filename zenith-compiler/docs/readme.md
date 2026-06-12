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

Based on the [C99 standard](https://en.wikipedia.org/wiki/C99), but with the following changes:

* True and false keywords
