export type ZenithModuleFactory = (options?: {
  locateFile?: (path: string) => string;
}) => Promise<ZenithEmscriptenModule>;

export type ZenithEmscriptenModule = {
  cwrap: <TReturn>(
    ident: string,
    returnType: "number" | "string" | null,
    argTypes: Array<"number" | "string">
  ) => TReturn;
};

export type ZenithApi = {
  add: (lhs: number, rhs: number) => number;
  reset: () => void;
  step: () => void;
  version: () => number;
};

let zenithApiPromise: Promise<ZenithApi> | null = null;

async function importZenithFactory() {
  const response = await fetch("/zenith-emulator.js");
  if (!response.ok) {
    throw new Error(
      `Failed to fetch Zenith emulator module: ${response.status} ${response.statusText}`
    );
  }

  const source = await response.text();
  const blob = new Blob([source], { type: "text/javascript" });
  const blobUrl = URL.createObjectURL(blob);

  try {
    const module = (await import(/* @vite-ignore */ blobUrl)) as {
      default: ZenithModuleFactory;
    };

    return module.default;
  } finally {
    URL.revokeObjectURL(blobUrl);
  }
}

export async function loadZenithModule(): Promise<ZenithApi> {
  if (zenithApiPromise !== null) {
    return zenithApiPromise;
  }

  zenithApiPromise = (async () => {
    const createModule = await importZenithFactory();
    const module = await createModule({
      locateFile: (path) => `/${path}`,
    });

    return {
      add: module.cwrap<ZenithApi["add"]>("zenith_emulator_add", "number", [
        "number",
        "number",
      ]),
      reset: module.cwrap<ZenithApi["reset"]>(
        "zenith_emulator_reset",
        null,
        []
      ),
      step: module.cwrap<ZenithApi["step"]>("zenith_emulator_step", null, []),
      version: module.cwrap<ZenithApi["version"]>(
        "zenith_emulator_version",
        "number",
        []
      ),
    };
  })();

  try {
    return await zenithApiPromise;
  } catch (error) {
    zenithApiPromise = null;
    throw error;
  }
}
