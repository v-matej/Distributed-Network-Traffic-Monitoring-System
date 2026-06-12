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

type ConfirmDialogState =
  | {
      kind: "delete-agent";
      agent: KnownAgent;
    }
  | {
      kind: "clear-all";
    }
  | null;

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
  const [isConfirmingAction, setIsConfirmingAction] = useState(false);
  const [confirmDialog, setConfirmDialog] = useState<ConfirmDialogState>(null);
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

  function handleDelete(agent: KnownAgent) {
    setConfirmDialog({
      kind: "delete-agent",
      agent,
    });
  }

  function handleClearAll() {
    setConfirmDialog({
      kind: "clear-all",
    });
  }

  async function handleConfirmDialogAction() {
    if (!confirmDialog) {
      return;
    }

    setIsConfirmingAction(true);
    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      if (confirmDialog.kind === "delete-agent") {
        const agentId = confirmDialog.agent.agent_id;

        await deleteAgent(agentId);

        setAgents((current) =>
          current.filter((agent) => agent.agent_id !== agentId),
        );
        setAgentStatuses((current) => {
          const next = { ...current };
          delete next[agentId];
          return next;
        });

        setSuccessMessage(`Agent ${agentId} deleted.`);
      } else {
        await clearAgents();

        setAgents([]);
        setAgentStatuses({});
        setSuccessMessage("All known agents cleared.");
      }

      setConfirmDialog(null);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to complete action",
      );
    } finally {
      setIsConfirmingAction(false);
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
            onClick={handleClearAll}
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
                          onClick={() => handleDelete(agent)}
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

      {confirmDialog && (
        <ConfirmDialog
          dialog={confirmDialog}
          isConfirming={isConfirmingAction}
          onCancel={() => setConfirmDialog(null)}
          onConfirm={() => void handleConfirmDialogAction()}
        />
      )}
    </div>
  );
}

type ConfirmDialogProps = {
  dialog: ConfirmDialogState;
  isConfirming: boolean;
  onCancel: () => void;
  onConfirm: () => void;
};

function ConfirmDialog({
  dialog,
  isConfirming,
  onCancel,
  onConfirm,
}: ConfirmDialogProps) {
  if (!dialog) {
    return null;
  }

  const isDeleteAgent = dialog.kind === "delete-agent";
  const agent = isDeleteAgent ? dialog.agent : null;

  const title = isDeleteAgent ? "Delete agent" : "Clear all agents";
  const confirmLabel = isDeleteAgent ? "Delete agent" : "Clear all";

  const description = isDeleteAgent
    ? "This removes the agent from the controller registry and deletes its stored controller-side captures."
    : "This removes every registered agent and clears persisted controller-side capture storage.";

  return (
    <div
      className="fixed inset-0 z-50 grid place-items-center bg-black/75 px-4 backdrop-blur-sm"
      role="dialog"
      aria-modal="true"
      aria-labelledby="agent-confirm-title"
    >
      <div className="w-full max-w-[520px] border border-console-red/45 bg-console-panel p-5 shadow-console">
        <div className="mb-4 border-b border-console-border pb-4">
          <div className="font-mono text-[11px] uppercase tracking-[0.18em] text-console-red">
            Destructive action
          </div>

          <h3
            id="agent-confirm-title"
            className="mb-0 mt-2 text-[22px] font-semibold uppercase tracking-[0.06em] text-console-text"
          >
            {title}
          </h3>

          <p className="mt-2 text-sm text-console-muted">{description}</p>
        </div>

        {agent && (
          <dl className="detail-list mb-4">
            <div>
              <dt>Agent ID</dt>
              <dd>
                <code>{agent.agent_id}</code>
              </dd>
            </div>

            <div>
              <dt>Name</dt>
              <dd>{agent.display_name || "Unnamed agent"}</dd>
            </div>

            <div>
              <dt>Endpoint</dt>
              <dd>
                {agent.host}:{agent.port}
              </dd>
            </div>
          </dl>
        )}

        {!agent && (
          <div className="mb-4 border border-console-red/30 bg-console-red/10 p-3 font-mono text-[12px] uppercase tracking-[0.06em] text-console-red">
            This action affects all known agents.
          </div>
        )}

        <div className="flex flex-wrap justify-end gap-2">
          <button
            className="secondary-button"
            type="button"
            onClick={onCancel}
            disabled={isConfirming}
          >
            Cancel
          </button>

          <button
            className="danger-button"
            type="button"
            onClick={onConfirm}
            disabled={isConfirming}
          >
            {isConfirming ? "Working..." : confirmLabel}
          </button>
        </div>
      </div>
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
