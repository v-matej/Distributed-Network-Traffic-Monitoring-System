import { useState } from "react";
import {
  BrowserRouter,
  NavLink,
  Navigate,
  Route,
  Routes,
  useLocation,
} from "react-router-dom";
import {
  IconActivityHeartbeat,
  IconAdjustmentsHorizontal,
  IconAntennaBars5,
  IconDatabase,
  IconHomeStats,
  IconMoon,
  IconNetwork,
  IconRadar,
  IconServer2,
  IconSettings,
  IconShieldCheck,
  IconSun,
} from "@tabler/icons-react";

import { DashboardPage } from "./pages/DashboardPage";
import { AgentsPage } from "./pages/AgentsPage";
import { AgentDetailPage } from "./pages/AgentDetailPage";
import { CapturesPage } from "./pages/CapturesPage";
import { CaptureDetailPage } from "./pages/CaptureDetailPage";
import { PacketAnalysisPage } from "./pages/PacketAnalysisPage";
import { SettingsPage } from "./pages/SettingsPage";
import { getStoredTheme, setStoredTheme, type UiTheme } from "./lib/theme";

const navItems = [
  {
    to: "/dashboard",
    label: "Dashboard",
    icon: IconHomeStats,
  },
  {
    to: "/agents",
    label: "Agents",
    icon: IconNetwork,
  },
  {
    to: "/captures",
    label: "Captures",
    icon: IconDatabase,
  },
  {
    to: "/settings",
    label: "Settings",
    icon: IconSettings,
  },
];

const PAGE_TITLES: Record<string, string> = {
  "/dashboard": "Dashboard",
  "/agents": "Agent Registry",
  "/captures": "Capture Sessions",
  "/settings": "Settings",
};

function getPageTitle(pathname: string) {
  if (pathname.startsWith("/agents/")) {
    return "Agent Details";
  }

  if (pathname.includes("/packets")) {
    return "Packet Inspection";
  }

  if (pathname.startsWith("/captures/")) {
    return "Capture Details";
  }

  return PAGE_TITLES[pathname] ?? "Packet Capture";
}

function AppLayout() {
  const location = useLocation();
  const pageTitle = getPageTitle(location.pathname);

  const [theme, setTheme] = useState<UiTheme>(() => getStoredTheme());

  function toggleTheme() {
    const nextTheme: UiTheme = theme === "dark" ? "light" : "dark";

    setTheme(nextTheme);
    setStoredTheme(nextTheme);
  }

  return (
    <div className="app-shell">
      <aside className="sidebar">
        <div className="brand">
          <div className="brand-mark">
            <IconRadar size={24} stroke={1.8} />
          </div>

          <div>
            <div className="brand-title">PKTCAPTURE</div>
            <div className="brand-subtitle">Controller v0.4.1</div>
          </div>
        </div>

        <div className="sidebar-status-card">
          <div className="live-dot-row">
            <span className="live-dot" />
            <span>Controller online</span>
          </div>

          <div className="sidebar-status-meta">
            <span>REST API</span>
            <strong>READY</strong>
          </div>
        </div>

        <div className="sidebar-section-label">Monitor</div>

        <nav className="nav">
          {navItems.map((item) => {
            const Icon = item.icon;

            return (
              <NavLink key={item.to} to={item.to}>
                <Icon size={17} stroke={1.8} />
                <span>{item.label}</span>
              </NavLink>
            );
          })}
        </nav>

        <div className="sidebar-section-label">System</div>

        <div className="sidebar-mini-grid">
          <div>
            <IconServer2 size={15} />
            <span>Mode</span>
            <strong>LAN</strong>
          </div>

          <div>
            <IconShieldCheck size={15} />
            <span>State</span>
            <strong>Stable</strong>
          </div>
        </div>

        <button
          type="button"
          className="theme-toggle-btn"
          onClick={toggleTheme}
          aria-label={
            theme === "dark" ? "Switch to light theme" : "Switch to dark theme"
          }
        >
          {theme === "dark" ? <IconSun size={15} /> : <IconMoon size={15} />}
          <span>{theme === "dark" ? "Light mode" : "Dark mode"}</span>
        </button>
      </aside>

      <main className="main">
        <header className="topbar">
          <div>
            <div className="eyebrow">
              <IconAntennaBars5 size={15} />
              Distributed Network Traffic Monitoring
            </div>

            <h1>{pageTitle}</h1>
            <p>
              Manage agents, launch captures, inspect PCAP sessions, and monitor
              controller state.
            </p>
          </div>

          <div className="topbar-actions">
            <div className="status-pill">
              <IconActivityHeartbeat size={15} />
              Controller API
            </div>

            <div className="status-pill status-pill-cyan">
              <IconAdjustmentsHorizontal size={15} />
              Capture ready
            </div>
          </div>
        </header>

        <section className="content">
          <Routes>
            <Route path="/" element={<Navigate to="/dashboard" replace />} />
            <Route path="/dashboard" element={<DashboardPage />} />
            <Route path="/agents" element={<AgentsPage />} />
            <Route path="/agents/:agentId" element={<AgentDetailPage />} />
            <Route path="/captures" element={<CapturesPage />} />
            <Route
              path="/captures/:agentId/:captureId/packets"
              element={<PacketAnalysisPage />}
            />
            <Route
              path="/captures/:agentId/:captureId"
              element={<CaptureDetailPage />}
            />
            <Route path="/settings" element={<SettingsPage />} />
          </Routes>
        </section>
      </main>
    </div>
  );
}

function App() {
  return (
    <BrowserRouter>
      <AppLayout />
    </BrowserRouter>
  );
}

export default App;
