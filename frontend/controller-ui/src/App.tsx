import { NavLink, Navigate, Route, Routes } from "react-router-dom";
import { BrowserRouter } from "react-router-dom";

import { DashboardPage } from "./pages/DashboardPage";
import { AgentsPage } from "./pages/AgentsPage";
import { AgentDetailPage } from "./pages/AgentDetailPage";

function App() {
  return (
    <BrowserRouter>
      <div className="app-shell">
        <aside className="sidebar">
          <div className="brand">
            <div className="brand-mark">NT</div>
            <div>
              <div className="brand-title">Net Monitor</div>
              <div className="brand-subtitle">Controller</div>
            </div>
          </div>

          <nav className="nav">
            <NavLink to="/dashboard">Dashboard</NavLink>
            <NavLink to="/agents">Agents</NavLink>
            <NavLink to="/captures">Captures</NavLink>
            <NavLink to="/settings">Settings</NavLink>
          </nav>
        </aside>

        <main className="main">
          <header className="topbar">
            <div>
              <h1>Distributed Network Traffic Monitoring</h1>
              <p>Controller dashboard for managing agents and captures.</p>
            </div>

            <div className="status-pill">
              Controller API
            </div>
          </header>

          <section className="content">
            <Routes>
              <Route path="/" element={<Navigate to="/dashboard" replace />} />
              <Route path="/dashboard" element={<DashboardPage />} />
              <Route path="/agents" element={<AgentsPage />} />
              <Route path="/agents/:agentId" element={<AgentDetailPage />} />

              <Route
                path="/captures"
                element={
                  <PlaceholderPage
                    title="Captures"
                    description="Global capture overview will be added after the agent detail flow."
                  />
                }
              />

              <Route
                path="/settings"
                element={
                  <PlaceholderPage
                    title="Settings"
                    description="Controller settings and storage options will be added later."
                  />
                }
              />
            </Routes>
          </section>
        </main>
      </div>
    </BrowserRouter>
  );
}

type PlaceholderPageProps = {
  title: string;
  description: string;
};

function PlaceholderPage({ title, description }: PlaceholderPageProps) {
  return (
    <div className="page-card">
      <h2>{title}</h2>
      <p>{description}</p>
    </div>
  );
}

export default App;