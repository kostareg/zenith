import * as React from "react";
import createModule, { type Emulator } from "@/wasm/zenith-emulator";

const ZenithEmulatorContext = React.createContext<Emulator | null>(null);

type ZenithEmulatorProviderProps = {
  children: React.ReactNode;
};

export function ZenithEmulatorProvider({
  children,
}: ZenithEmulatorProviderProps) {
  const [emulator, setEmulator] = React.useState<Emulator | null>(null);
  const [isLoading, setIsLoading] = React.useState(true);

  React.useEffect(() => {
    const load = async () => {
      const module = await createModule();
      const emulator = new module.Emulator();
      setEmulator(emulator);
      setIsLoading(false);
      console.info("zenith emulator successfully loaded");
    };

    load();
  }, []);

  return (
    <ZenithEmulatorContext.Provider value={emulator}>
      {children}
    </ZenithEmulatorContext.Provider>
  );
}

export function useZenithEmulator() {
  const context = React.useContext(ZenithEmulatorContext);
  if (context === undefined) {
    throw new Error(
      "useZenithEmulator must be used within a ZenithEmulatorProvider."
    );
  }

  return context;
}
