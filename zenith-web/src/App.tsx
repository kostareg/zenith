import { useZenithEmulator } from "@/components/zenith-emulator-provider";
import { Button } from "@/components/ui/button";
import { Input } from "./components/ui/input";
import { useState } from "react";
import { Field } from "./components/ui/field";

const REGISTER_COUNT = 32;

export function App() {
  const { emulator, error, isLoading } = useZenithEmulator();
  let [registers, setRegisters] = useState(emulator?.get_registers() ?? Array.from({ length: REGISTER_COUNT }, () => 0n));
  let sum: number | null = null;
  let version: number | null = null;
  let emulatorRenderError: Error | null = null;

  let [instruction, setInstruction] = useState(0);

  if (emulator) {
    try {
      sum = emulator.add(2, 3);
      version = emulator.version();
    } catch (caughtError) {
      emulatorRenderError =
        caughtError instanceof Error
          ? caughtError
          : new Error("Failed to read emulator state.");
    }
  }

  return (
    <div className="flex min-h-svh p-6">
      <div className="flex max-w-md min-w-0 flex-col gap-4 text-sm leading-loose">
        <div>
          <h1 className="font-medium">Zenith Web</h1>
          <p>WebAssembly-backed emulator state is now available via context.</p>
          {isLoading ? <p>Loading emulator...</p> : null}
          {error ? (
            <p className="text-destructive">
              Failed to load emulator: {error.message}
            </p>
          ) : null}
          {emulatorRenderError ? (
            <p className="text-destructive">
              Failed to read emulator state: {emulatorRenderError.message}
            </p>
          ) : null}
          {emulator ? (
            <>
              <p>2 + 3 = {sum}</p>
              <p>version: {version}</p>
              <Field orientation="horizontal">
                <Input value={"0x" + instruction.toString(16).toUpperCase()} onChange={(e) => setInstruction(parseInt(e.target.value.slice(2) || "0", 16))} />
                <Button className="mt-2" onClick={() => { emulator.step(instruction); setRegisters(emulator.get_registers()); }}>
                  Step emulator
                </Button>
              </Field>
              {registers.map((value, index) => (
                <p key={index}>
                  Register {index}: {value.toString()}
                </p>
              ))}
            </>
          ) : null}
        </div>
        <div className="font-mono text-xs text-muted-foreground">
          (Press <kbd>d</kbd> to toggle dark mode)
        </div>
      </div>
    </div>
  );
}

export default App;
