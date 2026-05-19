# Zenith Web

Inspired by [Godbolt](https://godbolt.org/) and [CPUlator](https://cpulator.01xz.net/), this is an online editor and emulator for Zenith. The emulator
found in zenith-emulator is built to WebAssembly and imported into this project.

This is a React, TypeScript, and Shadcn project, with a Codemirror editor.

## Setup

```bash
bun i
# ensure the wasm is built and included; see /zenith-emulator/docs/readme.md
bun run dev
```

## Adding components

To add components, run the following command:

```bash
bunx shadcn@latest add button
```

This will place the ui components in the `src/components` directory.

## Using components

To use the components, import them as follows:

```tsx
import { Button } from "@/components/ui/button";
```

## Formatting

To format the whole project (may result in malformed code), run:

```bash
bunx prettier . --write
```
