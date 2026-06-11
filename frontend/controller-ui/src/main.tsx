import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import "@fontsource/rajdhani/400.css";
import "@fontsource/rajdhani/500.css";
import "@fontsource/rajdhani/600.css";
import "@fontsource/share-tech-mono/400.css";

import App from "./App";
import "./index.css";
import "./styles/packet-inspection.css";

import { applyTheme, getStoredTheme } from "./lib/theme";

applyTheme(getStoredTheme());

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);