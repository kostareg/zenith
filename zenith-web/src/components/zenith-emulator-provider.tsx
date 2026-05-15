import * as React from "react";

type ZenithEmulator = {
  reset: () => void;
  step: (instruction: number) => void;
  getRegisters: () => Array<bigint>;
  delete: () => void;
};

type ZenithModule = {
  Emulator: {
    new (): ZenithEmulator;
  };
};

type ZenithModuleFactory = (options?: {
  locateFile?: (path: string) => string;
}) => Promise<ZenithModule>;

type ZenithEmulatorContextValue = {
  emulator: ZenithEmulator | null;
  error: Error | null;
  isLoading: boolean;
};

const ZenithEmulatorContext = React.createContext<
  ZenithEmulatorContextValue | undefined
>(undefined);

type ZenithEmulatorProviderProps = {
  children: React.ReactNode;
};

export function ZenithEmulatorProvider({
  children,
}: ZenithEmulatorProviderProps) {
  const [emulator, setEmulator] = React.useState<ZenithEmulator | null>(null);
  const [error, setError] = React.useState<Error | null>(null);
  const [isLoading, setIsLoading] = React.useState(true);

  React.useEffect(() => {
    let cancelled = false;
    let loadedEmulator: ZenithEmulator | null = null;

    void (async () => {
      try {
        const response = await fetch("/zenith-emulator.js");
        if (!response.ok) {
          throw new Error(
            `Failed to fetch Zenith emulator module: ${response.status} ${response.statusText}`
          );
        }

        const source = await response.text();
        const blob = new Blob([source], { type: "text/javascript" });
        const blobUrl = URL.createObjectURL(blob);
        let createModule: ZenithModuleFactory;

        try {
          ({ default: createModule } = (await import(
            /* @vite-ignore */ blobUrl
          )) as { default: ZenithModuleFactory });
        } finally {
          URL.revokeObjectURL(blobUrl);
        }

        const module = await createModule({
          locateFile: (path) => `/${path}`,
        });
        const nextEmulator = new module.Emulator();
        loadedEmulator = nextEmulator;

        if (cancelled) {
          nextEmulator.delete();
          return;
        }

        setEmulator(nextEmulator);
      } catch (caughtError) {
        if (cancelled) {
          return;
        }

        setError(
          caughtError instanceof Error
            ? caughtError
            : new Error("Failed to load the Zenith emulator module.")
        );
      } finally {
        if (!cancelled) {
          setIsLoading(false);
        }
      }
    })();

    return () => {
      cancelled = true;
      loadedEmulator?.delete();
    };
  }, []);

  const value = React.useMemo(
    () => ({
      emulator,
      error,
      isLoading,
    }),
    [emulator, error, isLoading]
  );

  return (
    <ZenithEmulatorContext.Provider value={value}>
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
