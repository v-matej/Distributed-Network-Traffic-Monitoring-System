import { useEffect, useMemo, useState } from "react";
import { Link } from "react-router-dom";

import {
  captureStatusClass,
  isActiveCapture,
  isCompletedCapture,
  isFailedCapture,
  isStoppableCapture,
} from "../lib/captureUtils";

import { formatBytes, formatUnixTime } from "../lib/format";

import { listAgents, listAgentCaptures, stopAgentCapture } from "../lib/api";

import type { KnownAgent, RemoteCaptureSessionInfo } from "../lib/api";

type GlobalCaptureRow = {
  agent: KnownAgent;
  capture: RemoteCaptureSessionInfo;
};

type AgentCaptureLoadError = {
  agent: KnownAgent;
  message: string;
};

type StatusFilter = "all" | "active" | "completed" | "failed";

export function CapturesPage() {
  const [agents, setAgents] = useState<KnownAgent[]>([]);
  const [captures, setCaptures] = useState<GlobalCaptureRow[]>([]);
  const [loadErrors, setLoadErrors] = useState<AgentCaptureLoadError[]>([]);

  const [statusFilter, setStatusFilter] = useState<StatusFilter>("all");
  const [selectedAgentId, setSelectedAgentId] = useState("all");
  const [searchText, setSearchText] = useState("");

  const [isLoading, setIsLoading] = useState(true);
  const [isStoppingKey, setIsStoppingKey] = useState<string | null>(null);
  const [lastLoadedAt, setLastLoadedAt] = useState<Date | null>(null);

  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  const activeCaptures = captures.filter((row) => isActiveCapture(row.capture));
  const completedCaptures = captures.filter((row) =>
    isCompletedCapture(row.capture),
  );
  const failedCaptures = captures.filter((row) => isFailedCapture(row.capture));

  const filteredCaptures = useMemo(() => {
    const normalizedSearch = searchText.trim().toLowerCase();

    return captures.filter((row) => {
      if (selectedAgentId !== "all" && row.agent.agent_id !== selectedAgentId) {
        return false;
      }

      if (statusFilter === "active" && !isActiveCapture(row.capture)) {
        return false;
      }

      if (statusFilter === "completed" && !isCompletedCapture(row.capture)) {
        return false;
      }

      if (statusFilter === "failed" && !isFailedCapture(row.capture)) {
        return false;
      }

      if (!normalizedSearch) {
        return true;
      }

      const haystack = [
        row.agent.agent_id,
        row.agent.display_name,
        row.agent.host,
        row.capture.capture_id,
        row.capture.status,
        row.capture.config.interface_name,
        row.capture.config.filter_expression,
        row.capture.config.output_file,
        row.capture.result.stop_reason,
        row.capture.result.error_message,
      ]
        .join(" ")
        .toLowerCase();

      return haystack.includes(normalizedSearch);
    });
  }, [captures, searchText, selectedAgentId, statusFilter]);

  async function loadGlobalCaptures(options: { silent?: boolean } = {}) {
    if (!options.silent) {
      setIsLoading(true);
      setErrorMessage(null);
    }

    try {
      const agentList = await listAgents();
      const captureRows: GlobalCaptureRow[] = [];
      const errors: AgentCaptureLoadError[] = [];

      await Promise.all(
        agentList.map(async (agent) => {
          try {
            const result = await listAgentCaptures(agent.agent_id);

            for (const capture of result.captures) {
              captureRows.push({ agent, capture });
            }
          } catch (error) {
            errors.push({
              agent,
              message:
                error instanceof Error
                  ? error.message
                  : "Failed to load captures for this agent",
            });
          }
        }),
      );

      captureRows.sort(
        (left, right) => right.capture.created_at - left.capture.created_at,
      );

      setAgents(agentList);
      setCaptures(captureRows);
      setLoadErrors(errors);
      setLastLoadedAt(new Date());
    } catch (error) {
      setErrorMessage(
        error instanceof Error
          ? error.message
          : "Failed to load global captures",
      );
    } finally {
      if (!options.silent) {
        setIsLoading(false);
      }
    }
  }

  async function handleStopCapture(row: GlobalCaptureRow) {
    const key = getCaptureKey(row);

    setIsStoppingKey(key);
    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      await stopAgentCapture(row.agent.agent_id, row.capture.capture_id);
      setSuccessMessage(`Stop requested for capture ${row.capture.capture_id}.`);
      await loadGlobalCaptures({ silent: true });
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to stop capture",
      );
    } finally {
      setIsStoppingKey(null);
    }
  }

  useEffect(() => {
    void loadGlobalCaptures();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    if (activeCaptures.length === 0) {
      return;
    }

    const intervalId = window.setInterval(() => {
      void loadGlobalCaptures({ silent: true });
    }, 2000);

    return () => window.clearInterval(intervalId);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [activeCaptures.length]);

  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <h2>Captures</h2>
          <p>Global overview of capture sessions across all known agents.</p>
        </div>

        <button
          className="secondary-button"
          onClick={() => void loadGlobalCaptures()}
          disabled={isLoading}
        >
          {isLoading ? "Refreshing..." : "Refresh"}
        </button>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}
      {successMessage && (
        <div className="alert alert-success">{successMessage}</div>
      )}

      {loadErrors.length > 0 && (
        <div className="alert alert-error">
          Some agents could not be queried:{" "}
          {loadErrors
            .map((item) => item.agent.display_name || item.agent.agent_id)
            .join(", ")}
        </div>
      )}

      <section className="metric-grid capture-metric-grid">
        <div className="metric-card">
          <span className="metric-label">Known agents</span>
          <strong>{agents.length}</strong>
          <p>Agents registered in the controller.</p>
        </div>

        <div className="metric-card">
          <span className="metric-label">Total captures</span>
          <strong>{captures.length}</strong>
          <p>All known sessions from all agents.</p>
        </div>

        <div className="metric-card">
          <span className="metric-label">Active captures</span>
          <strong>{activeCaptures.length}</strong>
          <p>Pending, starting, running, or stopping.</p>
        </div>

        <div className="metric-card">
          <span className="metric-label">Failed captures</span>
          <strong>{failedCaptures.length}</strong>
          <p>Sessions that failed on the agent.</p>
        </div>
      </section>

      <section className="page-card">
        <div className="section-heading">
          <div>
            <h3>Capture filters</h3>
            <p>
              Filter by status, agent, interface, capture ID, filter expression,
              or output file.
            </p>
          </div>
        </div>

        <div className="capture-filter-bar">
          <label className="field-label">
            Search
            <input
              value={searchText}
              placeholder="capture id, interface, filter, output file..."
              onChange={(event) => setSearchText(event.target.value)}
            />
          </label>

          <label className="field-label">
            Agent
            <select
              value={selectedAgentId}
              onChange={(event) => setSelectedAgentId(event.target.value)}
            >
              <option value="all">All agents</option>
              {agents.map((agent) => (
                <option key={agent.agent_id} value={agent.agent_id}>
                  {agent.display_name || agent.agent_id}
                </option>
              ))}
            </select>
          </label>
        </div>

        <div className="compact-option-grid capture-status-filter-row">
          <button
            type="button"
            className={`compact-filter-button ${
              statusFilter === "all" ? "active" : ""
            }`}
            onClick={() => setStatusFilter("all")}
          >
            All
          </button>

          <button
            type="button"
            className={`compact-filter-button protocol-tcp ${
              statusFilter === "active" ? "active" : ""
            }`}
            onClick={() => setStatusFilter("active")}
          >
            Active
          </button>

          <button
            type="button"
            className={`compact-filter-button protocol-udp ${
              statusFilter === "completed" ? "active" : ""
            }`}
            onClick={() => setStatusFilter("completed")}
          >
            Completed
          </button>

          <button
            type="button"
            className={`compact-filter-button protocol-icmp ${
              statusFilter === "failed" ? "active" : ""
            }`}
            onClick={() => setStatusFilter("failed")}
          >
            Failed
          </button>
        </div>
      </section>

      <section className="page-card">
        <div className="section-heading">
          <div>
            <h3>All captures</h3>
            <p>
              Showing {filteredCaptures.length} of {captures.length}.{" "}
              {completedCaptures.length} completed, {activeCaptures.length}{" "}
              active.
              {lastLoadedAt ? ` Last refresh: ${lastLoadedAt.toLocaleString()}.` : ""}
            </p>
          </div>
        </div>

        {isLoading ? (
          <div className="empty-state">
            <h3>Loading captures...</h3>
            <p>Reading agents and capture sessions from the controller.</p>
          </div>
        ) : captures.length === 0 ? (
          <div className="empty-state">
            <h3>No captures found</h3>
            <p>Start a capture from an agent detail page to see it here.</p>
          </div>
        ) : filteredCaptures.length === 0 ? (
          <div className="empty-state">
            <h3>No captures match the filters</h3>
            <p>Try clearing the search text or changing the status filter.</p>
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
                  <th>Filter</th>
                  <th>Packets</th>
                  <th>Bytes</th>
                  <th>Created</th>
                  <th />
                </tr>
              </thead>

              <tbody>
                {filteredCaptures.map((row) => {
                  const key = getCaptureKey(row);
                  const isStopping = key === isStoppingKey;

                  return (
                    <tr key={key}>
                      <td>
                        <div className="table-main-text">
                          {row.agent.display_name || "Unnamed agent"}
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

                      <td className="capture-filter-cell">
                        {row.capture.config.filter_expression ? (
                          <code>{row.capture.config.filter_expression}</code>
                        ) : (
                          <span className="muted-text">No filter</span>
                        )}
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

                        {isStoppableCapture(row.capture) && (
                          <button
                            className="small-button danger-text"
                            disabled={isStopping}
                            onClick={() => void handleStopCapture(row)}
                          >
                            {isStopping ? "Stopping..." : "Stop"}
                          </button>
                        )}
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

function getCaptureKey(row: GlobalCaptureRow) {
  return `${row.agent.agent_id}:${row.capture.capture_id}`;
}