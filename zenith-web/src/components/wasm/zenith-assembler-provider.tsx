import * as React from "react";
import createModule, { type Assembler } from "@/wasm/zenith-assembler";

const ZenithAssemblerContext = React.createContext<Assembler | null>(null);

type ZenithAssemblerProviderProps = {
    children: React.ReactNode;
};

export function ZenithAssemblerProvider({ children }: ZenithAssemblerProviderProps) {
    const [assembler, setAssembler] = React.useState<Assembler | null>(null);

    React.useEffect(() => {
        const load = async () => {
            const module = await createModule();
            const assembler = new module.Assembler();
            setAssembler(assembler);
            console.info("zenith assembler successfully loaded");
        };

        load();
    }, []);

    return <ZenithAssemblerContext.Provider value={assembler}>{children}</ZenithAssemblerContext.Provider>;
}

export function useZenithAssembler() {
    const context = React.useContext(ZenithAssemblerContext);
    if (context === undefined) {
        throw new Error("useZenithAssembler must be used within a ZenithAssemblerProvider.");
    }

    return context;
}
