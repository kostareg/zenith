/// <reference types="vite/client" />

declare module "/zenith-emulator.js" {
  type ZenithEmscriptenModule = {
    cwrap: <TReturn>(
      ident: string,
      returnType: "number" | "string" | null,
      argTypes: Array<"number" | "string">
    ) => TReturn;
  };

  type ZenithModuleFactory = (options?: {
    locateFile?: (path: string) => string;
  }) => Promise<ZenithEmscriptenModule>;

  const createModule: ZenithModuleFactory;

  export default createModule;
}
