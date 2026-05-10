export function DashboardPage() {
  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <h2>Dashboard</h2>
          <p>High-level status of the controller and registered agents.</p>
        </div>
      </section>

      <section className="metric-grid">
        <div className="metric-card">
          <span className="metric-label">Known agents</span>
          <strong>—</strong>
          <p>Will be loaded from the controller API.</p>
        </div>

        <div className="metric-card">
          <span className="metric-label">Online agents</span>
          <strong>—</strong>
          <p>Health polling will be added later.</p>
        </div>

        <div className="metric-card">
          <span className="metric-label">Running captures</span>
          <strong>—</strong>
          <p>Capture aggregation will come after agent detail.</p>
        </div>
      </section>
    </div>
  );
}