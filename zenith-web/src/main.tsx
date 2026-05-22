import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import "@/index.css";
import App from "@/App.tsx";
import { ZenithEmulatorProvider } from "@/components/wasm/zenith-emulator-provider.tsx";
import { ZenithAssemblerProvider } from "@/components/wasm/zenith-assembler-provider.tsx";

createRoot(document.getElementById("root")!).render(
    <StrictMode>
        <ZenithEmulatorProvider>
            <ZenithAssemblerProvider>
                <App />
            </ZenithAssemblerProvider>
        </ZenithEmulatorProvider>
    </StrictMode>
);
