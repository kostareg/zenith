import * as React from "react";
import createModule, { type Compiler } from "@/wasm/zenith-compiler";

const ZenithCompilerContext = React.createContext<Compiler | null>(null);

type ZenithCompilerProviderProps = {
    children: React.ReactNode;
};

export function ZenithCompilerProvider({ children }: ZenithCompilerProviderProps) {
    const [compiler, setCompiler] = React.useState<Compiler | null>(null);

    React.useEffect(() => {
        const load = async () => {
            const module = await createModule();
            const compiler = new module.Compiler();
            setCompiler(compiler);
            console.info("zenith compiler successfully loaded");
        };

        load();
    }, []);

    return <ZenithCompilerContext.Provider value={compiler}>{children}</ZenithCompilerContext.Provider>;
}

export function useZenithCompiler() {
    const context = React.useContext(ZenithCompilerContext);
    if (context === undefined) {
        throw new Error("useZenithCompiler must be used within a ZenithCompilerProvider.");
    }

    return context;
}
