import { useEffect, useMemo, useState } from "react";
import { Link } from "react-router-dom";

import {
  getAgentHealth,
  listAgentCaptures,
  listAgents,
} from "../lib/api";

import type {
  KnownAgent,
  KnownAgentWithHealth,
  RemoteCaptureSessionInfo,
} from "../lib/api";

type DashboardAgentHealth = {
  agent: KnownAgent;
  health: KnownAgentWithHealth | null;
  errorMessage: string | null;
};

type DashboardCaptureRow = {
  agent: KnownAgent;
  capture: RemoteCaptureSessionInfo;
};

export function DashboardPage() {
  const [agents, setAgents] = useState<KnownAgent[]>([]);
  const [agentHealth, setAgentHealth] = useState<DashboardAgentHealth[]>([]);
  const [captures, setCaptures] = useState<DashboardCaptureRow[]>([]);

  const [isLoading, setIsLoading] = useState(true);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [lastLoadedAt, setLastLoadedAt] = useState<Date | null>(null);

  const [errorMessage, setErrorMessage] = useState<string | null>(null);

  const onlineAgents = agentHealth.filter((item) =>
    item.health ? isHealthyStatus(item.health.health.status) : false,
  );

  const offlineAgents = agentHealth.filter((item) => !item.health);

  const activeCaptures = captures.filter((row) =>
    isActiveCapture(row.capture),
  );

  const completedCaptures = captures.filter((row) =>
    isCompletedCapture(row.capture),
  );

  const failedCaptures = captures.filter((row) =>
    isFailedCapture(row.capture),
  );

  const recentCaptures = useMemo(() => {
    return [...captures]
      .sort((left, right) => right.capture.created_at - left.capture.created_at)
      .slice(0, 6);
  }, [captures]);

  async function loadDashboard(options: { silent?: boolean } = {}) {
    if (!options.silent) {
      setIsRefreshing(true);
      setErrorMessage(null);
    }

    try {
      const agentList = await listAgents();

      const healthRows: DashboardAgentHealth[] = [];
      const captureRows: DashboardCaptureRow[] = [];

      await Promise.all(
        agentList.map(async (agent) => {
          const [healthResult, capturesResult] = await Promise.allSettled([
            getAgentHealth(agent.agent_id),
            listAgentCaptures(agent.agent_id),
          ]);

          if (healthResult.status === "fulfilled") {
            healthRows.push({
              agent,
              health: healthResult.value,
              errorMessage: null,
            });
          } else {
            healthRows.push({
              agent,
              health: null,
              errorMessage:
                healthResult.reason instanceof Error
                  ? healthResult.reason.message
                  : "Failed to read agent health",
            });
          }

          if (capturesResult.status === "fulfilled") {
            for (const capture of capturesResult.value.captures) {
              captureRows.push({
                agent,
                capture,
              });
            }
          }
        }),
      );

      healthRows.sort((left, right) =>
        getAgentDisplayName(left.agent).localeCompare(
          getAgentDisplayName(right.agent),
        ),
      );

      captureRows.sort(
        (left, right) => right.capture.created_at - left.capture.created_at,
      );

      setAgents(agentList);
      setAgentHealth(healthRows);
      setCaptures(captureRows);
      setLastLoadedAt(new Date());
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to load dashboard",
      );
    } finally {
      setIsLoading(false);

      if (!options.silent) {
        setIsRefreshing(false);
      }
    }
  }

  useEffect(() => {
    void loadDashboard();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    const intervalId = window.setInterval(() => {
      void loadDashboard({ silent: true });
    }, activeCaptures.length > 0 ? 5000 : 15000);

    return () => window.clearInterval(intervalId);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [activeCaptures.length]);

  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <h2>Dashboard</h2>
          <p>
            High-level status of the controller, registered agents, and capture
            activity.
          </p>
        </div>

        <button
          className="secondary-button"
          onClick={() => void loadDashboard()}
          disabled={isRefreshing}
        >
          {isRefreshing ? "Refreshing..." : "Refresh"}
        </button>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}

      {isLoading ? (
        <div className="page-card">
          <h3>Loading dashboard...</h3>
          <p>Reading agents, health status, and capture sessions.</p>
        </div>
      ) : (
        <>
          <section className="metric-grid dashboard-metric-grid">
            <div className="metric-card">
              <span className="metric-label">Known agents</span>
              <strong>{agents.length}</strong>
              <p>Registered controller agents.</p>
            </div>

            <div className="metric-card">
              <span className="metric-label">Online agents</span>
              <strong>{onlineAgents.length}</strong>
              <p>
                {offlineAgents.length} offline or unreachable.
              </p>
            </div>

            <div className="metric-card">
              <span className="metric-label">Active captures</span>
              <strong>{activeCaptures.length}</strong>
              <p>Pending, starting, running, or stopping.</p>
            </div>

            <div className="metric-card">
              <span className="metric-label">Total captures</span>
              <strong>{captures.length}</strong>
              <p>
                {completedCaptures.length} completed, {failedCaptures.length} failed.
              </p>
            </div>
          </section>

          <section className="dashboard-overview-grid">
            <div className="page-card">
              <div className="section-heading">
                <div>
                  <h3>Agent status</h3>
                  <p>
                    {lastLoadedAt
                      ? `Last refresh: ${lastLoadedAt.toLocaleString()}.`
                      : "Current agent health overview."}
                  </p>
                </div>

                <Link className="small-button" to="/agents">
                  Manage agents
                </Link>
              </div>

              {agentHealth.length === 0 ? (
                <div className="empty-state compact-empty-state">
                  <h3>No agents registered</h3>
                  <p>Add an agent to start monitoring.</p>
                </div>
              ) : (
                <div className="dashboard-agent-list">
                  {agentHealth.map((item) => (
                    <Link
                      className="dashboard-agent-card"
                      key={item.agent.agent_id}
                      to={`/agents/${item.agent.agent_id}`}
                    >
                      <div>
                        <strong>{getAgentDisplayName(item.agent)}</strong>
                        <span>
                          {item.agent.host}:{item.agent.port}
                        </span>
                      </div>

                      <span
                        className={`status-badge ${
                          item.health
                            ? statusClass(item.health.health.status)
                            : "status-danger"
                        }`}
                      >
                        {item.health ? item.health.health.status : "offline"}
                      </span>
                    </Link>
                  ))}
                </div>
              )}
            </div>

            <div className="page-card">
              <div className="section-heading">
                <div>
                  <h3>Capture summary</h3>
                  <p>Current capture activity across all agents.</p>
                </div>

                <Link className="small-button" to="/captures">
                  View captures
                </Link>
              </div>

              <div className="dashboard-summary-list">
                <div>
                  <span>Active</span>
                  <strong>{activeCaptures.length}</strong>
                </div>

                <div>
                  <span>Completed</span>
                  <strong>{completedCaptures.length}</strong>
                </div>

                <div>
                  <span>Failed</span>
                  <strong>{failedCaptures.length}</strong>
                </div>

                <div>
                  <span>Total bytes</span>
                  <strong>
                    {formatBytes(
                      captures.reduce(
                        (sum, row) => sum + row.capture.result.bytes_captured,
                        0,
                      ),
                    )}
                  </strong>
                </div>

                <div>
                  <span>Total packets</span>
                  <strong>
                    {captures.reduce(
                      (sum, row) => sum + row.capture.result.packets_captured,
                      0,
                    )}
                  </strong>
                </div>
              </div>
            </div>
          </section>

          <section className="page-card">
            <div className="section-heading">
              <div>
                <h3>Recent captures</h3>
                <p>Latest capture sessions reported by known agents.</p>
              </div>

              <Link className="small-button" to="/captures">
                Open captures
              </Link>
            </div>

            {recentCaptures.length === 0 ? (
              <div className="empty-state compact-empty-state">
                <h3>No captures yet</h3>
                <p>Start a capture from an agent detail page.</p>
              </div>
            ) : (
              <div className="table-wrap">
                <table className="data-table">
                  <thead>
                    <tr>
                      <th>Agent</th>
                      <th>Capture ID</th>
                      <th>Status</th>
                      <th>Interface</th>
                      <th>Packets</th>
                      <th>Bytes</th>
                      <th>Created</th>
                      <th />
                    </tr>
                  </thead>

                  <tbody>
                    {recentCaptures.map((row) => (
                      <tr key={`${row.agent.agent_id}:${row.capture.capture_id}`}>
                        <td>
                          <div className="table-main-text">
                            {getAgentDisplayName(row.agent)}
                          </div>
                          <div className="table-sub-text">
                            {row.agent.host}:{row.agent.port}
                          </div>
                        </td>

                        <td>
                          <code>{row.capture.capture_id}</code>
                        </td>

                        <td>
                          <span
                            className={`status-badge ${captureStatusClass(
                              row.capture.status,
                            )}`}
                          >
                            {row.capture.status}
                          </span>
                        </td>

                        <td>
                          <code>{row.capture.config.interface_name || "—"}</code>
                        </td>

                        <td>{row.capture.result.packets_captured}</td>
                        <td>{formatBytes(row.capture.result.bytes_captured)}</td>
                        <td>{formatUnixTime(row.capture.created_at)}</td>

                        <td className="table-actions">
                          <Link
                            className="small-button"
                            to={`/captures/${row.agent.agent_id}/${row.capture.capture_id}`}
                          >
                            Open
                          </Link>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </section>
        </>
      )}
    </div>
  );
}

function getAgentDisplayName(agent: KnownAgent) {
  return agent.display_name || agent.agent_id;
}

function isHealthyStatus(status: string) {
  const normalized = status.toLowerCase();
  return (
    normalized === "ok" ||
    normalized === "healthy" ||
    normalized === "running"
  );
}

function isActiveCapture(capture: RemoteCaptureSessionInfo) {
  const status = capture.status.toLowerCase();

  return (
    status === "pending" ||
    status === "starting" ||
    status === "running" ||
    status === "stopping"
  );
}

function isCompletedCapture(capture: RemoteCaptureSessionInfo) {
  const status = capture.status.toLowerCase();
  return status === "completed" || status === "stopped";
}

function isFailedCapture(capture: RemoteCaptureSessionInfo) {
  return capture.status.toLowerCase() === "failed";
}

function statusClass(status: string) {
  const normalized = status.toLowerCase();

  if (isHealthyStatus(normalized)) {
    return "status-good";
  }

  if (normalized === "warning") {
    return "status-warning";
  }

  return "status-neutral";
}

function captureStatusClass(status: string) {
  const normalized = status.toLowerCase();

  if (normalized === "completed" || normalized === "stopped") {
    return "status-good";
  }

  if (
    normalized === "pending" ||
    normalized === "running" ||
    normalized === "starting"
  ) {
    return "status-active";
  }

  if (normalized === "stopping" || normalized === "finalizing") {
    return "status-warning";
  }

  if (normalized === "failed") {
    return "status-danger";
  }

  return "status-neutral";
}

function formatUnixTime(value: number) {
  if (!value) {
    return "—";
  }

  return new Date(value * 1000).toLocaleString();
}

function formatBytes(value: number) {
  if (!Number.isFinite(value) || value <= 0) {
    return "0 B";
  }

  const units = ["B", "KB", "MB", "GB"];
  let size = value;
  let unitIndex = 0;

  while (size >= 1024 && unitIndex < units.length - 1) {
    size /= 1024;
    unitIndex += 1;
  }

  return `${size.toFixed(unitIndex === 0 ? 0 : 1)} ${units[unitIndex]}`;
}