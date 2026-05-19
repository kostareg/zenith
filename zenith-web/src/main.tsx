import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import "./index.css";
import App from "./App.tsx";
import { ZenithEmulatorProvider } from "@/components/zenith-emulator-provider.tsx";

createRoot(document.getElementById("root")!).render(
    <StrictMode>
        <ZenithEmulatorProvider>
            <App />
        </ZenithEmulatorProvider>
    </StrictMode>
);
