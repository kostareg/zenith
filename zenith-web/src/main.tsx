import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import "./index.css";
import App from "./App.tsx";
import { ThemeProvider } from "@/components/theme-provider.tsx";
import { ZenithEmulatorProvider } from "@/components/zenith-emulator-provider.tsx";

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <ThemeProvider>
      <ZenithEmulatorProvider>
        <App />
      </ZenithEmulatorProvider>
    </ThemeProvider>
  </StrictMode>
);
