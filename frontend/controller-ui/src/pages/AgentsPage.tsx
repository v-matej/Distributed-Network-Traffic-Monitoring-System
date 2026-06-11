import { useEffect, useState } from "react";
import type { FormEvent } from "react";
import { Link } from "react-router-dom";
import { IconRefresh } from "@tabler/icons-react";

import { statusClass } from "../lib/agentUtils";
import { formatUnixTime } from "../lib/format";

import {
  addAgent,
  clearAgents,
  deleteAgent,
  getAgentHealth,
  listAgents,
} from "../lib/api";

import type { KnownAgent, KnownAgentWithHealth } from "../lib/api";

type FormState = {
  display_name: string;
  host: string;
  port: string;
};

type AgentStatusState = {
  label: string;
  className: string;
  title: string;
  lastCheckedAt: Date | null;
};

const initialFormState: FormState = {
  display_name: "",
  host: "127.0.0.1",
  port: "8080",
};

const checkingStatus: AgentStatusState = {
  label: "checking",
  className: "status-neutral",
  title: "Health check in progress",
  lastCheckedAt: null,
};

export function AgentsPage() {
  const [agents, setAgents] = useState<KnownAgent[]>([]);
  const [agentStatuses, setAgentStatuses] = useState<
    Record<string, AgentStatusState>
  >({});
  const [form, setForm] = useState<FormState>(initialFormState);
  const [isLoading, setIsLoading] = useState(true);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [isRefreshingStatuses, setIsRefreshingStatuses] = useState(false);
  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  async function refreshAgents() {
    setIsLoading(true);
    setErrorMessage(null);

    try {
      const result = await listAgents();
      setAgents(result);
      await refreshAgentStatuses(result);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to load agents",
      );
    } finally {
      setIsLoading(false);
    }
  }

  async function refreshAgentStatuses(agentList = agents) {
    if (agentList.length === 0) {
      setAgentStatuses({});
      return;
    }

    setIsRefreshingStatuses(true);

    setAgentStatuses((current) => {
      const next = { ...current };

      for (const agent of agentList) {
        if (!next[agent.agent_id]) {
          next[agent.agent_id] = checkingStatus;
        }
      }

      return next;
    });

    const results = await Promise.all(
      agentList.map(async (agent) => {
        try {
          const health = await getAgentHealth(agent.agent_id);
          return {
            agent,
            status: makeOnlineStatus(health),
          };
        } catch (error) {
          return {
            agent,
            status: makeOfflineStatus(error),
          };
        }
      }),
    );

    setAgentStatuses((current) => {
      const next = { ...current };

      for (const result of results) {
        next[result.agent.agent_id] = result.status;
      }

      return next;
    });

    setIsRefreshingStatuses(false);
  }

  useEffect(() => {
    void refreshAgents();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (agents.length === 0) {
      return;
    }

    const intervalId = window.setInterval(() => {
      void refreshAgentStatuses();
    }, 15000);

    return () => window.clearInterval(intervalId);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agents]);

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();

    setIsSubmitting(true);
    setErrorMessage(null);
    setSuccessMessage(null);

    const parsedPort = Number(form.port);

    if (!form.host.trim()) {
      setErrorMessage("Host is required.");
      setIsSubmitting(false);
      return;
    }

    if (!Number.isInteger(parsedPort) || parsedPort <= 0 || parsedPort > 65535) {
      setErrorMessage("Port must be a number between 1 and 65535.");
      setIsSubmitting(false);
      return;
    }

    try {
      const created = await addAgent({
        display_name: form.display_name.trim(),
        host: form.host.trim(),
        port: parsedPort,
      });

      setAgents((current) => [...current, created]);
      setAgentStatuses((current) => ({
        ...current,
        [created.agent_id]: checkingStatus,
      }));
      setForm(initialFormState);
      setSuccessMessage(`Agent ${created.agent_id} added.`);

      await refreshAgentStatuses([...agents, created]);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to add agent",
      );
    } finally {
      setIsSubmitting(false);
    }
  }

  async function handleDelete(agentId: string) {
    const shouldDelete = window.confirm(`Delete agent ${agentId}?`);
    if (!shouldDelete) {
      return;
    }

    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      await deleteAgent(agentId);
      setAgents((current) => current.filter((agent) => agent.agent_id !== agentId));
      setAgentStatuses((current) => {
        const next = { ...current };
        delete next[agentId];
        return next;
      });
      setSuccessMessage(`Agent ${agentId} deleted.`);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to delete agent",
      );
    }
  }

  async function handleClearAll() {
    const shouldClear = window.confirm(
      "Clear all known agents? This also clears persisted controller storage.",
    );

    if (!shouldClear) {
      return;
    }

    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      await clearAgents();
      setAgents([]);
      setAgentStatuses({});
      setSuccessMessage("All known agents cleared.");
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to clear agents",
      );
    }
  }

  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <h2>Agents</h2>
          <p>Manage known agents registered in the controller.</p>
        </div>

        <button
          className="secondary-button"
          onClick={() => void refreshAgents()}
          disabled={isLoading}
        >
          <IconRefresh size={16} />
          {isLoading ? "Refreshing..." : "Refresh"}
        </button>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}
      {successMessage && <div className="alert alert-success">{successMessage}</div>}

      <section className="page-card">
        <h3>Add agent</h3>

        <form className="form-grid" onSubmit={handleSubmit}>
          <label>
            Display name
            <input
              value={form.display_name}
              placeholder="local-agent"
              onChange={(event) =>
                setForm((current) => ({
                  ...current,
                  display_name: event.target.value,
                }))
              }
            />
          </label>

          <label>
            Host
            <input
              value={form.host}
              placeholder="127.0.0.1"
              onChange={(event) =>
                setForm((current) => ({
                  ...current,
                  host: event.target.value,
                }))
              }
            />
          </label>

          <label>
            Port
            <input
              value={form.port}
              placeholder="8080"
              inputMode="numeric"
              onChange={(event) =>
                setForm((current) => ({
                  ...current,
                  port: event.target.value,
                }))
              }
            />
          </label>

          <div className="form-actions">
            <button className="primary-button" type="submit" disabled={isSubmitting}>
              {isSubmitting ? "Adding..." : "Add agent"}
            </button>
          </div>
        </form>
      </section>

      <section className="page-card">
        <div className="section-heading">
          <div>
            <h3>Known agents</h3>
            <p>
              {agents.length} registered agent{agents.length === 1 ? "" : "s"}
              {agents.length > 0
                ? ` · statuses ${isRefreshingStatuses ? "refreshing" : "checked"}`
                : ""}
            </p>
          </div>

          <button
            className="danger-button"
            onClick={() => void handleClearAll()}
            disabled={agents.length === 0}
          >
            Clear all
          </button>
        </div>

        {isLoading ? (
          <div className="empty-state">
            <h3>Loading agents...</h3>
            <p>Reading known agents from the controller.</p>
          </div>
        ) : agents.length === 0 ? (
          <div className="empty-state">
            <h3>No agents registered</h3>
            <p>Add an agent by host and port to start managing captures.</p>
          </div>
        ) : (
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Name</th>
                  <th>Endpoint</th>
                  <th>Status</th>
                  <th>Created</th>
                  <th />
                </tr>
              </thead>

              <tbody>
                {agents.map((agent) => {
                  const agentStatus =
                    agentStatuses[agent.agent_id] ?? checkingStatus;

                  return (
                    <tr key={agent.agent_id}>
                      <td>
                        <code>{agent.agent_id}</code>
                      </td>

                      <td>{agent.display_name || "Unnamed agent"}</td>

                      <td>
                        {agent.host}:{agent.port}
                      </td>

                      <td>
                        <span
                          className={`status-badge ${agentStatus.className}`}
                          title={agentStatus.title}
                        >
                          {agentStatus.label}
                        </span>

                        {agentStatus.lastCheckedAt && (
                          <div className="table-sub-text">
                            {agentStatus.lastCheckedAt.toLocaleTimeString()}
                          </div>
                        )}
                      </td>

                      <td>{formatUnixTime(agent.created_at)}</td>

                      <td className="table-actions">
                        <Link
                          className="small-button"
                          to={`/agents/${agent.agent_id}`}
                        >
                          Open
                        </Link>

                        <button
                          className="small-button danger-text"
                          onClick={() => void handleDelete(agent.agent_id)}
                        >
                          Delete
                        </button>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </section>
    </div>
  );
}

function makeOnlineStatus(health: KnownAgentWithHealth): AgentStatusState {
  const normalized = health.health.status.toLowerCase();

  return {
    label: normalized === "ok" ? "OK" : health.health.status,
    className: statusClass(health.health.status),
    title: `Agent responded. Hostname: ${health.health.hostname || "unknown"}`,
    lastCheckedAt: new Date(),
  };
}

function makeOfflineStatus(error: unknown): AgentStatusState {
  return {
    label: "OFFLINE",
    className: "status-danger",
    title: error instanceof Error ? error.message : "Agent health check failed",
    lastCheckedAt: new Date(),
  };
}