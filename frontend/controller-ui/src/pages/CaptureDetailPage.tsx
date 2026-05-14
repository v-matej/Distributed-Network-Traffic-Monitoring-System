import { useEffect, useMemo, useState } from "react";
import { Link, useParams } from "react-router-dom";

import {
  getAgentCapture,
  getAgentCaptureDownloadUrl,
  stopAgentCapture,
} from "../lib/api";

import {
  captureStatusClass,
  getCaptureDerivedStats,
  isActiveCapture,
  isStoppableCapture,
} from "../lib/captureUtils";

import {
  formatBytes,
  formatDurationSeconds,
  formatUnixTime,
} from "../lib/format";

import type {
  KnownAgent,
  RemoteCaptureSessionInfo,
} from "../lib/api";

export function CaptureDetailPage() {
  const { agentId, captureId } = useParams();

  const [agent, setAgent] = useState<KnownAgent | null>(null);
  const [capture, setCapture] = useState<RemoteCaptureSessionInfo | null>(null);

  const [isLoading, setIsLoading] = useState(true);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [isStopping, setIsStopping] = useState(false);
  const [lastLoadedAt, setLastLoadedAt] = useState<Date | null>(null);

  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  const derivedStats = useMemo(() => {
    return capture ? getCaptureDerivedStats(capture) : null;
  }, [capture]);

  async function loadCapture(options: { silent?: boolean } = {}) {
    if (!agentId || !captureId) {
      return;
    }

    if (!options.silent) {
      setIsRefreshing(true);
      setErrorMessage(null);
    }

    try {
      const result = await getAgentCapture(agentId, captureId);
      setAgent(result.agent);
      setCapture(result.capture);
      setLastLoadedAt(new Date());
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to load capture",
      );
    } finally {
      if (!options.silent) {
        setIsRefreshing(false);
        setIsLoading(false);
      }
    }
  }

  async function handleStopCapture() {
    if (!agentId || !captureId) {
      return;
    }

    setIsStopping(true);
    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      await stopAgentCapture(agentId, captureId);
      setSuccessMessage(`Stop requested for capture ${captureId}.`);
      await loadCapture({ silent: true });
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to stop capture",
      );
    } finally {
      setIsStopping(false);
    }
  }

  useEffect(() => {
    void loadCapture();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agentId, captureId]);

  useEffect(() => {
    if (!capture || !isActiveCapture(capture)) {
      return;
    }

    const intervalId = window.setInterval(() => {
      void loadCapture({ silent: true });
    }, 2000);

    return () => window.clearInterval(intervalId);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [capture?.status, agentId, captureId]);

  if (!agentId || !captureId) {
    return (
      <div className="page-card">
        <h2>Capture not found</h2>
        <p>Missing agent id or capture id.</p>
        <Link className="text-link" to="/captures">
          Back to captures
        </Link>
      </div>
    );
  }

  return (
    <div className="page-stack">
      <section className="page-header">
        <div>
          <Link className="text-link" to="/captures">
            ← Back to captures
          </Link>
          <h2>Capture detail</h2>
          <p>
            {agent
              ? `${agent.display_name || agent.agent_id} · ${agent.host}:${agent.port}`
              : "Loading capture agent..."}
          </p>
        </div>

        <div className="capture-detail-actions">
          {agent && capture && !isActiveCapture(capture) && (
            <a className="primary-button"
            href={getAgentCaptureDownloadUrl(agent.agent_id, capture.capture_id)}
            download> Download PCAP </a>
          )}

          <button
            className="secondary-button"
            onClick={() => void loadCapture()}
            disabled={isRefreshing}
          >
            {isRefreshing ? "Refreshing..." : "Refresh"}
          </button>

          {capture && isStoppableCapture(capture) && (
            <button
              className="danger-button"
              onClick={() => void handleStopCapture()}
              disabled={isStopping}
            >
              {isStopping ? "Stopping..." : "Stop capture"}
            </button>
          )}
        </div>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}
      {successMessage && (
        <div className="alert alert-success">{successMessage}</div>
      )}

      {isLoading || !capture || !agent || !derivedStats ? (
        <div className="page-card">
          <h3>Loading capture...</h3>
          <p>Reading capture data from the controller.</p>
        </div>
      ) : (
        <>
          <section className="capture-detail-hero">
            <div>
              <span className="active-capture-label">Capture session</span>
              <h3>{capture.capture_id}</h3>
              <p>
                Last refresh:{" "}
                {lastLoadedAt ? lastLoadedAt.toLocaleString() : "—"}
              </p>
            </div>

            <span className={`status-badge ${captureStatusClass(capture.status)}`}>
              {capture.status}
            </span>
          </section>

          <section className="metric-grid capture-metric-grid">
            <div className="metric-card">
              <span className="metric-label">Packets</span>
              <strong>{capture.result.packets_captured}</strong>
              <p>Total packets captured.</p>
            </div>

            <div className="metric-card">
              <span className="metric-label">Bytes</span>
              <strong>{formatBytes(capture.result.bytes_captured)}</strong>
              <p>Total captured payload size.</p>
            </div>

            <div className="metric-card">
              <span className="metric-label">Avg throughput</span>
              <strong>{derivedStats.averageThroughput}</strong>
              <p>Calculated after capture finalizes.</p>
            </div>

            <div className="metric-card">
              <span className="metric-label">Packet rate</span>
              <strong>{derivedStats.packetRate}</strong>
              <p>Average packets per second.</p>
            </div>
          </section>

          <section className="capture-detail-grid">
            <div className="capture-detail-section">
              <h4>Session</h4>

              <dl className="compact-detail-list">
                <div>
                  <dt>Status</dt>
                  <dd>
                    <span
                      className={`status-badge ${captureStatusClass(
                        capture.status,
                      )}`}
                    >
                      {capture.status}
                    </span>
                  </dd>
                </div>

                <div>
                  <dt>Capture ID</dt>
                  <dd>
                    <code>{capture.capture_id}</code>
                  </dd>
                </div>

                <div>
                  <dt>Stop requested</dt>
                  <dd>{capture.stop_requested ? "Yes" : "No"}</dd>
                </div>

                <div>
                  <dt>Duration</dt>
                  <dd>
                    {derivedStats.durationSeconds !== null
                      ? formatDurationSeconds(derivedStats.durationSeconds)
                      : "Available after completion"}
                  </dd>
                </div>
              </dl>
            </div>

            <div className="capture-detail-section">
              <h4>Agent</h4>

              <dl className="compact-detail-list">
                <div>
                  <dt>Name</dt>
                  <dd>{agent.display_name || "Unnamed agent"}</dd>
                </div>

                <div>
                  <dt>Agent ID</dt>
                  <dd>
                    <code>{agent.agent_id}</code>
                  </dd>
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
            </div>

            <div className="capture-detail-section">
              <h4>Configuration</h4>

              <dl className="compact-detail-list">
                <div>
                  <dt>Interface</dt>
                  <dd>
                    <code>{capture.config.interface_name || "—"}</code>
                  </dd>
                </div>

                <div>
                  <dt>Output file</dt>
                  <dd>
                    <code>{capture.config.output_file || "—"}</code>
                  </dd>
                </div>

                <div>
                  <dt>Filter</dt>
                  <dd>
                    <code>
                      {capture.config.filter_expression || "No packet filter"}
                    </code>
                  </dd>
                </div>

                <div>
                  <dt>Time limit</dt>
                  <dd>
                    {capture.config.duration_seconds > 0
                      ? `${capture.config.duration_seconds} seconds`
                      : "No time limit"}
                  </dd>
                </div>

                <div>
                  <dt>Packet limit</dt>
                  <dd>
                    {capture.config.packet_count > 0
                      ? `${capture.config.packet_count} packets`
                      : "No packet limit"}
                  </dd>
                </div>

                <div>
                  <dt>Live output</dt>
                  <dd>{capture.config.live_output ? "Enabled" : "Disabled"}</dd>
                </div>
              </dl>
            </div>

            <div className="capture-detail-section">
              <h4>Result</h4>

              <dl className="compact-detail-list">
                <div>
                  <dt>Success</dt>
                  <dd>{capture.result.success ? "Yes" : "No"}</dd>
                </div>

                <div>
                  <dt>Packets</dt>
                  <dd>{capture.result.packets_captured}</dd>
                </div>

                <div>
                  <dt>Bytes</dt>
                  <dd>{formatBytes(capture.result.bytes_captured)}</dd>
                </div>

                <div>
                  <dt>Stop reason</dt>
                  <dd>{capture.result.stop_reason || "—"}</dd>
                </div>

                <div>
                  <dt>Avg packet size</dt>
                  <dd>{derivedStats.averagePacketSize}</dd>
                </div>
              </dl>
            </div>

            <div className="capture-detail-section">
              <h4>Timeline</h4>

              <dl className="compact-detail-list">
                <div>
                  <dt>Session created</dt>
                  <dd>{formatUnixTime(capture.created_at)}</dd>
                </div>

                <div>
                  <dt>Session started</dt>
                  <dd>{formatUnixTime(capture.started_at)}</dd>
                </div>

                <div>
                  <dt>Session finished</dt>
                  <dd>{formatUnixTime(capture.finished_at)}</dd>
                </div>

                <div>
                  <dt>Result start</dt>
                  <dd>{formatUnixTime(capture.result.start_time)}</dd>
                </div>

                <div>
                  <dt>Result end</dt>
                  <dd>{formatUnixTime(capture.result.end_time)}</dd>
                </div>
              </dl>
            </div>

            <div className="capture-detail-section">
              <h4>Diagnostics</h4>

              {capture.result.error_message ? (
                <div className="diagnostic-box diagnostic-error">
                  {capture.result.error_message}
                </div>
              ) : (
                <div className="diagnostic-box">
                  No error message reported for this capture.
                </div>
              )}

              {isActiveCapture(capture) && (
                <div className="future-action-box">
                  Live throughput needs backend progress counters. Current
                  throughput is calculated from final capture result values.
                </div>
              )}

              {!isActiveCapture(capture) && (
                <div className="future-action-box">
                  PCAP download is available from the page header. Viewer controls can be added here later.
                </div>
              )}
            </div>
          </section>
        </>
      )}
    </div>
  );
}