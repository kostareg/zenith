import * as React from "react";

import {
  loadZenithModule,
  type ZenithApi,
} from "@/lib/zenith-emulator";

type ZenithEmulatorContextValue = {
  emulator: ZenithApi | null;
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
  const [emulator, setEmulator] = React.useState<ZenithApi | null>(null);
  const [error, setError] = React.useState<Error | null>(null);
  const [isLoading, setIsLoading] = React.useState(true);

  React.useEffect(() => {
    let cancelled = false;

    void (async () => {
      try {
        const zenith = await loadZenithModule();
        if (cancelled) {
          return;
        }

        setEmulator(zenith);
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
