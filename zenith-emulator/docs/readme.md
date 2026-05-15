# Zenith Emulator

This directory contains the C++ emulator project for Zenith. It is organized as:

- `lib/zenith-emulator`: the reusable emulator library and public headers.
- `apps/zenith-emulator`: a native executable that links against the library.

## Native build

```bash
cmake --preset native-debug
cmake --build --preset native-debug
./build/native-debug/apps/zenith-emulator/zenith-emulator-app
```

## WebAssembly build

Install and activate Emscripten first so the `EMSCRIPTEN` environment variable points at the SDK, then run:

```bash
emcmake cmake --preset wasm-debug
cmake --build --preset wasm-debug
cp ./build/wasm-debug/zenith-emulator.{js,wasm} ../zenith-web/public/
```

The WebAssembly build emits `build/wasm-debug/zenith-emulator.js` and the paired `.wasm` binary. The generated module exports the embind `Emulator` class:

```js
import createModule from "./zenith-emulator.js";

const module = await createModule();
const emulator = new module.Emulator();

emulator.reset();
emulator.step(0x04);
console.log(emulator.getRegisters());
console.log(module.Emulator.version());
```
