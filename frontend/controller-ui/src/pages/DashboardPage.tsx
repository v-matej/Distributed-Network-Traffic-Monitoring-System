import { useEffect, useMemo, useState } from "react";
import { Link } from "react-router-dom";
import {
  IconActivity,
  IconArrowRight,
  IconBinaryTree,
  IconDatabase,
  IconDownload,
  IconNetwork,
  IconRefresh,
  IconServer2,
  IconTerminal2,
  IconWifi,
} from "@tabler/icons-react";

import {
  captureStatusClass,
  isActiveCapture,
  isCompletedCapture,
  isFailedCapture,
} from "../lib/captureUtils";

import {
  getAgentDisplayName,
  isHealthyStatus,
  statusClass,
} from "../lib/agentUtils";

import { formatBytes, formatUnixTime } from "../lib/format";

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

  const totalPackets = captures.reduce(
    (sum, row) => sum + row.capture.result.packets_captured,
    0,
  );

  const totalBytes = captures.reduce(
    (sum, row) => sum + row.capture.result.bytes_captured,
    0,
  );

  const recentCaptures = useMemo(() => {
    return [...captures]
      .sort((left, right) => right.capture.created_at - left.capture.created_at)
      .slice(0, 6);
  }, [captures]);

  const activityLines = useMemo(() => {
    if (recentCaptures.length === 0) {
      return [
        "controller: waiting for capture sessions",
        "agent registry: ready",
        "pcap storage: download proxy online",
      ];
    }

    return recentCaptures.slice(0, 5).map((row) => {
      const agentName = getAgentDisplayName(row.agent);
      const iface = row.capture.config.interface_name || "unknown-iface";
      return `${row.capture.status.toUpperCase()} ${row.capture.capture_id} on ${agentName}/${iface}`;
    });
  }, [recentCaptures]);

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
    <div className="page-stack animate-softIn">
      <section className="console-hero">
        <div className="console-hero-content">
          <div className="console-kicker">
            <span className="live-dot" />
            Live controller overview
          </div>

          <h2>Network capture operations</h2>

          <p>
            Real-time control plane for registered agents, remote captures, PCAP download,
            and session inspection.
          </p>

          <div className="console-hero-meta">
            <span>
              <IconServer2 size={15} />
              {agents.length} agents
            </span>

            <span>
              <IconActivity size={15} />
              {activeCaptures.length} active captures
            </span>

            <span>
              <IconDatabase size={15} />
              {formatBytes(totalBytes)} stored result data
            </span>
          </div>
        </div>

        <div className="console-hero-actions">
          <button
            className="secondary-button"
            onClick={() => void loadDashboard()}
            disabled={isRefreshing}
          >
            <IconRefresh size={16} />
            {isRefreshing ? "Refreshing..." : "Refresh"}
          </button>

          <Link className="primary-button" to="/agents">
            <IconNetwork size={16} />
            Manage agents
          </Link>
        </div>
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
            <DashboardMetric
              icon={<IconNetwork size={18} />}
              label="Known agents"
              value={agents.length.toString()}
              description={`${onlineAgents.length} online, ${offlineAgents.length} offline`}
              accent="green"
            />

            <DashboardMetric
              icon={<IconWifi size={18} />}
              label="Active captures"
              value={activeCaptures.length.toString()}
              description="Pending, starting, running, or stopping"
              accent="cyan"
            />

            <DashboardMetric
              icon={<IconBinaryTree size={18} />}
              label="Packets"
              value={totalPackets.toLocaleString()}
              description="Captured across completed sessions"
              accent="amber"
            />

            <DashboardMetric
              icon={<IconDownload size={18} />}
              label="Capture data"
              value={formatBytes(totalBytes)}
              description={`${completedCaptures.length} completed sessions`}
              accent="purple"
            />
          </section>

          <section className="dashboard-console-grid">
            <div className="page-card dashboard-command-panel">
              <div className="panel-title-row">
                <div>
                  <h3>Agent status</h3>
                  <p>
                    {lastLoadedAt
                      ? `Last refresh: ${lastLoadedAt.toLocaleString()}`
                      : "Current agent health overview"}
                  </p>
                </div>

                <Link className="small-button" to="/agents">
                  Open
                  <IconArrowRight size={14} />
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
                      <div className="agent-card-left">
                        <span className={item.health ? "live-dot" : "dead-dot"} />

                        <div>
                          <strong>{getAgentDisplayName(item.agent)}</strong>
                          <span>
                            {item.agent.host}:{item.agent.port}
                          </span>
                        </div>
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

            <div className="page-card dashboard-command-panel">
              <div className="panel-title-row">
                <div>
                  <h3>Controller activity</h3>
                  <p>Recent control-plane events derived from capture history.</p>
                </div>

                <IconTerminal2 className="panel-icon" size={20} />
              </div>

              <div className="terminal-feed">
                {activityLines.map((line, index) => (
                  <div key={`${line}-${index}`} className="terminal-line">
                    <span>{new Date().toLocaleTimeString()}</span>
                    <code>{line}</code>
                  </div>
                ))}
              </div>

              <div className="dashboard-summary-list terminal-summary-list">
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
                  <strong>{formatBytes(totalBytes)}</strong>
                </div>
              </div>
            </div>
          </section>

          <section className="page-card">
            <div className="panel-title-row">
              <div>
                <h3>Recent captures</h3>
                <p>Latest capture sessions reported by known agents.</p>
              </div>

              <Link className="small-button" to="/captures">
                Open captures
                <IconArrowRight size={14} />
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

type DashboardMetricProps = {
  icon: React.ReactNode;
  label: string;
  value: string;
  description: string;
  accent: "green" | "cyan" | "amber" | "purple" | "red";
};

function DashboardMetric({
  icon,
  label,
  value,
  description,
  accent,
}: DashboardMetricProps) {
  return (
    <div className={`metric-card console-metric console-metric-${accent}`}>
      <div className="metric-topline">
        <span>{icon}</span>
        <span>{label}</span>
      </div>

      <strong>{value}</strong>
      <p>{description}</p>
    </div>
  );
}