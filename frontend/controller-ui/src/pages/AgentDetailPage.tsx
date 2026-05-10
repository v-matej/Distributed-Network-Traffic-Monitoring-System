import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";

import {
  getAgent,
  getAgentHealth,
  getAgentInterfaces,
} from "../lib/api";
import type {
  AgentInterfacesResponse,
  KnownAgent,
  KnownAgentWithHealth,
} from "../lib/api";

export function AgentDetailPage() {
  const { agentId } = useParams();

  const [agent, setAgent] = useState<KnownAgent | null>(null);
  const [health, setHealth] = useState<KnownAgentWithHealth | null>(null);
  const [interfaces, setInterfaces] = useState<AgentInterfacesResponse | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [isRefreshingHealth, setIsRefreshingHealth] = useState(false);
  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [lastHealthCheckAt, setLastHealthCheckAt] = useState<Date | null>(null);

  async function loadAgentDetail() {
    if (!agentId) {
      setErrorMessage("Missing agent id.");
      setIsLoading(false);
      return;
    }

    setIsLoading(true);
    setErrorMessage(null);

    try {
      const [agentResult, healthResult, interfacesResult] = await Promise.all([
        getAgent(agentId),
        getAgentHealth(agentId),
        getAgentInterfaces(agentId),
      ]);

      setAgent(agentResult);
      setHealth(healthResult);
      setLastHealthCheckAt(new Date());
      setInterfaces(interfacesResult);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to load agent detail",
      );
    } finally {
      setIsLoading(false);
    }
  }

  async function refreshHealth() {
    if (!agentId) {
      return;
    }

    setIsRefreshingHealth(true);
    setErrorMessage(null);

    try {
      const healthResult = await getAgentHealth(agentId);
      setHealth(healthResult);
      setLastHealthCheckAt(new Date());
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to refresh health",
      );
    } finally {
      setIsRefreshingHealth(false);
    }
  }

  useEffect(() => {
    void loadAgentDetail();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agentId]);

  if (!agentId) {
    return (
      <div className="page-card">
        <h2>Agent not found</h2>
        <p>No agent id was provided.</p>
        <Link className="text-link" to="/agents">
          Back to agents
        </Link>
      </div>
    );
  }

  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <Link className="text-link" to="/agents">
            ← Back to agents
          </Link>
          <h2>{agent?.display_name || agentId}</h2>
          <p>
            {agent ? `${agent.host}:${agent.port}` : "Loading agent endpoint..."}
          </p>
        </div>

        <button
          className="secondary-button"
          onClick={() => void loadAgentDetail()}
          disabled={isLoading}
        >
          {isLoading ? "Refreshing..." : "Refresh all"}
        </button>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}

      {isLoading ? (
        <div className="page-card">
          <h3>Loading agent detail...</h3>
          <p>Reading agent metadata, health, and interfaces from the controller.</p>
        </div>
      ) : (
        <>
          <section className="two-column">
            <div className="page-card">
              <div className="section-heading">
                <div>
                  <h3>Health</h3>
                  <p>Current status reported by the agent.</p>
                </div>

                <button
                  className="small-button"
                  onClick={() => void refreshHealth()}
                  disabled={isRefreshingHealth}
                >
                  {isRefreshingHealth ? "Checking..." : "Check"}
                </button>
              </div>

              {health ? (
                <dl className="detail-list">
                  <div>
                    <dt>Status</dt>
                    <dd className="status-row">
                        <span className={`status-badge ${statusClass(health.health.status)}`}>
                        {health.health.status}
                        </span>

                        {lastHealthCheckAt && (
                        <span className="status-time">
                            Last checked {lastHealthCheckAt.toLocaleString()}
                        </span>
                        )}
                    </dd>
                </div>

                  <div>
                    <dt>Agent name</dt>
                    <dd>{health.health.agent_name || "—"}</dd>
                  </div>

                  <div>
                    <dt>Hostname</dt>
                    <dd>{health.health.hostname || "—"}</dd>
                  </div>

                  <div>
                    <dt>Version</dt>
                    <dd>{health.health.version || "—"}</dd>
                  </div>
                </dl>
              ) : (
                <p className="muted-text">No health data available.</p>
              )}
            </div>

            <div className="page-card">
              <h3>Agent metadata</h3>

              {agent ? (
                <dl className="detail-list">
                  <div>
                    <dt>Agent ID</dt>
                    <dd>
                      <code>{agent.agent_id}</code>
                    </dd>
                  </div>

                  <div>
                    <dt>Display name</dt>
                    <dd>{agent.display_name || "Unnamed agent"}</dd>
                  </div>

                  <div>
                    <dt>Endpoint</dt>
                    <dd>
                      {agent.host}:{agent.port}
                    </dd>
                  </div>

                  <div>
                    <dt>Created</dt>
                    <dd>{formatUnixTime(agent.created_at)}</dd>
                  </div>
                </dl>
              ) : (
                <p className="muted-text">No agent metadata available.</p>
              )}
            </div>
          </section>

          <section className="page-card">
            <div className="section-heading">
              <div>
                <h3>Interfaces</h3>
                <p>
                  {interfaces?.interfaces.length ?? 0} interface
                  {(interfaces?.interfaces.length ?? 0) === 1 ? "" : "s"} available.
                </p>
              </div>
            </div>

            {!interfaces || interfaces.interfaces.length === 0 ? (
              <div className="empty-state">
                <h3>No interfaces found</h3>
                <p>The agent did not return any capture interfaces.</p>
              </div>
            ) : (
              <div className="table-wrap">
                <table className="data-table">
                  <thead>
                    <tr>
                      <th>Name</th>
                      <th>Description</th>
                    </tr>
                  </thead>

                  <tbody>
                    {interfaces.interfaces.map((iface) => (
                      <tr key={iface.name}>
                        <td>
                          <code>{iface.name}</code>
                        </td>
                        <td>{iface.description || "—"}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </section>

          <section className="page-card">
            <h3>Captures</h3>
            <p className="muted-text">
              Next step: add start/list/stop capture controls for this agent.
            </p>
          </section>
        </>
      )}
    </div>
  );
}

function formatUnixTime(value: number) {
  if (!value) {
    return "—";
  }

  return new Date(value * 1000).toLocaleString();
}

function statusClass(status: string) {
  const normalized = status.toLowerCase();

  if (normalized === "ok" || normalized === "healthy" || normalized === "running") {
    return "status-good";
  }

  if (normalized === "warning") {
    return "status-warning";
  }

  return "status-neutral";
}