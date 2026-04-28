import { useZenithEmulator } from "@/components/zenith-emulator-provider";
import { Button } from "@/components/ui/button";

export function App() {
  const { emulator, error, isLoading } = useZenithEmulator();

  const sum = emulator?.add(2, 3) ?? null;
  const version = emulator?.version() ?? null;

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
          {emulator ? (
            <>
              <p>2 + 3 = {sum}</p>
              <p>version: {version}</p>
              <Button className="mt-2" onClick={() => emulator.step()}>
                Step emulator
              </Button>
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
