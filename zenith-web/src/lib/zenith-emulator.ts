export type ZenithModuleFactory = (options?: {
  locateFile?: (path: string) => string;
}) => Promise<ZenithEmscriptenModule>;

export type ZenithEmscriptenModule = {
  cwrap: <TReturn>(
    ident: string,
    returnType: "number" | "string" | "Array<number>" | null,
    argTypes: Array<"number" | "string">
  ) => TReturn;
  _zenith_emulator_version: () => number | bigint;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAP64: BigInt64Array;
};

export type ZenithApi = {
  add: (lhs: number, rhs: number) => number;
  reset: () => void;
  step: (instruction?: number) => void;
  get_registers: () => Array<bigint>;
  version: () => number;
};

let zenithApiPromise: Promise<ZenithApi> | null = null;
const REGISTER_COUNT = 32;

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
    const getRegisters = module.cwrap<(registersPtr: number, registerCount: number) => void>(
      "zenith_emulator_get_registers",
      null,
      ["number", "number"]
    );
    const step = module.cwrap<(instruction: number) => void>(
      "zenith_emulator_step",
      null,
      ["number"]
    );

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
      step: (instruction = 0) => {
        step(instruction);
      },
      get_registers: () => {
        const registersPtr = module._malloc(
          REGISTER_COUNT * BigInt64Array.BYTES_PER_ELEMENT
        );

        try {
          getRegisters(registersPtr, REGISTER_COUNT);

          const registersOffset = registersPtr / BigInt64Array.BYTES_PER_ELEMENT;
          return Array.from(
            module.HEAP64.subarray(
              registersOffset,
              registersOffset + REGISTER_COUNT
            )
          );
        } finally {
          module._free(registersPtr);
        }
      },
      version: () => {
        const packedVersion = module._zenith_emulator_version();
        return typeof packedVersion === "bigint"
          ? Number(packedVersion)
          : packedVersion;
      },
    };
  })();

  try {
    return await zenithApiPromise;
  } catch (error) {
    zenithApiPromise = null;
    throw error;
  }
}
