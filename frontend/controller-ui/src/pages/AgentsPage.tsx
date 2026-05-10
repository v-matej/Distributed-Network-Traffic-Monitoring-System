import { useEffect, useState } from "react";
import type { FormEvent } from "react";
import { Link } from "react-router-dom";

import {
  addAgent,
  clearAgents,
  deleteAgent,
  listAgents,
} from "../lib/api";
import type { KnownAgent } from "../lib/api";

type FormState = {
  display_name: string;
  host: string;
  port: string;
};

const initialFormState: FormState = {
  display_name: "",
  host: "127.0.0.1",
  port: "8080",
};

export function AgentsPage() {
  const [agents, setAgents] = useState<KnownAgent[]>([]);
  const [form, setForm] = useState<FormState>(initialFormState);
  const [isLoading, setIsLoading] = useState(true);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  async function refreshAgents() {
    setIsLoading(true);
    setErrorMessage(null);

    try {
      const result = await listAgents();
      setAgents(result);
    } catch (error) {
      setErrorMessage(error instanceof Error ? error.message : "Failed to load agents");
    } finally {
      setIsLoading(false);
    }
  }

  useEffect(() => {
    void refreshAgents();
  }, []);

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
      setForm(initialFormState);
      setSuccessMessage(`Agent ${created.agent_id} added.`);
    } catch (error) {
      setErrorMessage(error instanceof Error ? error.message : "Failed to add agent");
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
      setSuccessMessage(`Agent ${agentId} deleted.`);
    } catch (error) {
      setErrorMessage(error instanceof Error ? error.message : "Failed to delete agent");
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
      setSuccessMessage("All known agents cleared.");
    } catch (error) {
      setErrorMessage(error instanceof Error ? error.message : "Failed to clear agents");
    }
  }

  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <h2>Agents</h2>
          <p>Manage known agents registered in the controller.</p>
        </div>

        <button className="secondary-button" onClick={() => void refreshAgents()}>
          Refresh
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
            <p>{agents.length} registered agent{agents.length === 1 ? "" : "s"}</p>
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
                  <th>Created</th>
                  <th />
                </tr>
              </thead>

              <tbody>
                {agents.map((agent) => (
                  <tr key={agent.agent_id}>
                    <td>
                      <code>{agent.agent_id}</code>
                    </td>
                    <td>{agent.display_name || "Unnamed agent"}</td>
                    <td>
                      {agent.host}:{agent.port}
                    </td>
                    <td>{formatUnixTime(agent.created_at)}</td>
                    <td className="table-actions">
                      <Link className="small-button" to={`/agents/${agent.agent_id}`}>
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
                ))}
              </tbody>
            </table>
          </div>
        )}
      </section>
    </div>
  );
}

function formatUnixTime(value: number) {
  if (!value) {
    return "—";
  }

  return new Date(value * 1000).toLocaleString();
}