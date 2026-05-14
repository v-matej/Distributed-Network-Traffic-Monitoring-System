import { useEffect, useState } from "react";

import { clearAgents, listAgents } from "../lib/api";

import type { KnownAgent } from "../lib/api";

export function SettingsPage() {
  const [agents, setAgents] = useState<KnownAgent[]>([]);
  const [confirmText, setConfirmText] = useState("");

  const [isLoading, setIsLoading] = useState(true);
  const [isClearing, setIsClearing] = useState(false);

  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  async function loadSettings() {
    setIsLoading(true);
    setErrorMessage(null);

    try {
      const result = await listAgents();
      setAgents(result);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to load settings",
      );
    } finally {
      setIsLoading(false);
    }
  }

  async function handleClearAgents() {
    if (confirmText !== "CLEAR") {
      setErrorMessage("Type CLEAR before clearing controller storage.");
      return;
    }

    setIsClearing(true);
    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      const previousCount = agents.length;

      await clearAgents();

      setAgents([]);
      setConfirmText("");
      setSuccessMessage(
        `Controller storage cleared. Removed ${previousCount} registered agent${
          previousCount === 1 ? "" : "s"
        }.`,
      );
    } catch (error) {
      setErrorMessage(
        error instanceof Error
          ? error.message
          : "Failed to clear controller storage",
      );
    } finally {
      setIsClearing(false);
    }
  }

  useEffect(() => {
    void loadSettings();
  }, []);

  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <h2>Settings</h2>
          <p>Controller storage and frontend runtime information.</p>
        </div>

        <button
          className="secondary-button"
          onClick={() => void loadSettings()}
          disabled={isLoading}
        >
          {isLoading ? "Refreshing..." : "Refresh"}
        </button>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}
      {successMessage && (
        <div className="alert alert-success">{successMessage}</div>
      )}

      <section className="two-column">
        <div className="page-card">
          <div className="section-heading">
            <div>
              <h3>Controller storage</h3>
              <p>Registered agents persisted by the controller.</p>
            </div>
          </div>

          {isLoading ? (
            <p className="muted-text">Loading controller storage...</p>
          ) : (
            <dl className="detail-list">
              <div>
                <dt>Known agents</dt>
                <dd>{agents.length}</dd>
              </div>

              <div>
                <dt>Storage type</dt>
                <dd>Controller-managed persistent registry</dd>
              </div>

              <div>
                <dt>Clear endpoint</dt>
                <dd>
                  <code>DELETE /api/agents</code>
                </dd>
              </div>
            </dl>
          )}
        </div>

        <div className="page-card">
          <div className="section-heading">
            <div>
              <h3>Frontend runtime</h3>
              <p>Current frontend-to-controller API configuration.</p>
            </div>
          </div>

          <dl className="detail-list">
            <div>
              <dt>API prefix</dt>
              <dd>
                <code>/api</code>
              </dd>
            </div>

            <div>
              <dt>Dev proxy</dt>
              <dd>Configured by Vite during development</dd>
            </div>

            <div>
              <dt>Refresh model</dt>
              <dd>Manual refresh plus polling on active captures</dd>
            </div>
          </dl>
        </div>
      </section>

      <section className="page-card danger-zone">
        <div className="section-heading">
          <div>
            <h3>Danger zone</h3>
            <p>
              Clear all registered agents from controller storage. This does not
              delete PCAP files from remote agents.
            </p>
          </div>
        </div>

        <div className="settings-danger-grid">
          <label className="field-label">
            Type CLEAR to confirm
            <input
              value={confirmText}
              placeholder="CLEAR"
              onChange={(event) => setConfirmText(event.target.value)}
            />
          </label>

          <button
            className="danger-button"
            disabled={isClearing || confirmText !== "CLEAR"}
            onClick={() => void handleClearAgents()}
          >
            {isClearing ? "Clearing..." : "Clear controller storage"}
          </button>
        </div>
      </section>
    </div>
  );
}