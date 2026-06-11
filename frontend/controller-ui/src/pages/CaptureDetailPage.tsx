import { useEffect, useMemo, useState } from "react";
import { Link, useParams } from "react-router-dom";

import { CollapsiblePanel } from "../components/CollapsiblePanel";

import {
  analyzeControllerStoredCapture,
  fetchAgentCaptureToController,
  getAgentCapture,
  getAgentCaptureDownloadUrl,
  getControllerStoredCapture,
  getControllerStoredCaptureDownloadUrl,
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
  ControllerStoredCaptureInfo,
  KnownAgent,
  PcapAnalysisCounter,
  PcapAnalysisResult,
  RemoteCaptureSessionInfo,
} from "../lib/api";

export function CaptureDetailPage() {
  const { agentId, captureId } = useParams();

  const [agent, setAgent] = useState<KnownAgent | null>(null);
  const [capture, setCapture] = useState<RemoteCaptureSessionInfo | null>(null);
  const [storedCapture, setStoredCapture] =
    useState<ControllerStoredCaptureInfo | null>(null);
  const [analysis, setAnalysis] = useState<PcapAnalysisResult | null>(null);

  const [isLoading, setIsLoading] = useState(true);
  const [isRefreshing, setIsRefreshing] = useState(false);
  const [isStopping, setIsStopping] = useState(false);
  const [isFetchingToController, setIsFetchingToController] = useState(false);
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [lastLoadedAt, setLastLoadedAt] = useState<Date | null>(null);

  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  const derivedStats = useMemo(() => {
    return capture ? getCaptureDerivedStats(capture) : null;
  }, [capture]);

  async function loadStoredCaptureInfo(options: { silent?: boolean } = {}) {
    if (!agentId || !captureId) {
      return;
    }

    try {
      const result = await getControllerStoredCapture(agentId, captureId);
      setStoredCapture(result.stored_capture);
    } catch {
      if (!options.silent) {
        setStoredCapture(null);
      }
    }
  }

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

      void loadStoredCaptureInfo({ silent: true });
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

  async function handleFetchToController() {
    if (!agentId || !captureId) {
      return;
    }

    setIsFetchingToController(true);
    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      const result = await fetchAgentCaptureToController(agentId, captureId);
      setStoredCapture(result.stored_capture);
      setAnalysis(null);
      setSuccessMessage(`Capture ${captureId} fetched to controller storage.`);
    } catch (error) {
      setErrorMessage(
        error instanceof Error
          ? error.message
          : "Failed to fetch capture to controller",
      );
    } finally {
      setIsFetchingToController(false);
    }
  }

  async function handleAnalyzeCapture() {
    if (!agentId || !captureId) {
      return;
    }

    setIsAnalyzing(true);
    setErrorMessage(null);
    setSuccessMessage(null);

    try {
      const result = await analyzeControllerStoredCapture(agentId, captureId);
      setAnalysis(result.analysis);
      setSuccessMessage(`Capture ${captureId} analyzed successfully.`);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to analyze capture",
      );
    } finally {
      setIsAnalyzing(false);
    }
  }

  useEffect(() => {
    void loadCapture();
    void loadStoredCaptureInfo();

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
              ? `${agent.display_name || agent.agent_id} · ${agent.host}:${
                  agent.port
                }`
              : "Loading capture agent..."}
          </p>
        </div>

        <div className="capture-detail-actions">
          {agent && capture && !isActiveCapture(capture) && (
            <>
              <a
                className="primary-button"
                href={
                  storedCapture?.exists
                    ? getControllerStoredCaptureDownloadUrl(
                        agent.agent_id,
                        capture.capture_id,
                      )
                    : getAgentCaptureDownloadUrl(
                        agent.agent_id,
                        capture.capture_id,
                      )
                }
                download
              >
                Download PCAP
              </a>

              {!storedCapture?.exists && (
                <button
                  className="secondary-button"
                  onClick={() => void handleFetchToController()}
                  disabled={isFetchingToController}
                >
                  {isFetchingToController ? "Fetching..." : "Fetch to controller"}
                </button>
              )}

              {storedCapture?.exists && (
                <>
                  <span className="status-badge status-good">
                    Stored on controller
                  </span>

                  <button
                    className="secondary-button"
                    onClick={() => void handleAnalyzeCapture()}
                    disabled={isAnalyzing}
                  >
                    {isAnalyzing ? "Analyzing..." : "Analyze"}
                  </button>

                  <Link
                    className="primary-button"
                    to={`/captures/${agent.agent_id}/${capture.capture_id}/packets`}
                  >
                    Packet view
                  </Link>
                </>
              )}
            </>
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

            <span
              className={`status-badge ${captureStatusClass(capture.status)}`}
            >
              {capture.status}
            </span>
          </section>

          <section className="capture-summary-grid">
            <div className="capture-summary-card">
              <span>Status</span>
              <strong>{capture.status}</strong>
              <p>Current session state.</p>
            </div>

            <div className="capture-summary-card">
              <span>Duration</span>
              <strong>
                {derivedStats.durationSeconds !== null
                  ? formatDurationSeconds(derivedStats.durationSeconds)
                  : "pending"}
              </strong>
              <p>Runtime based on result timestamps.</p>
            </div>

            <div className="capture-summary-card">
              <span>Controller storage</span>
              <strong>{storedCapture?.exists ? "stored" : "not stored"}</strong>
              <p>
                {storedCapture?.exists
                  ? formatBytes(storedCapture.file_size_bytes)
                  : "Fetch PCAP to enable inspection."}
              </p>
            </div>

            <div className="capture-summary-card">
              <span>Agent</span>
              <strong>{agent.display_name || agent.agent_id}</strong>
              <p>
                {agent.host}:{agent.port}
              </p>
            </div>
          </section>

          {analysis && (
            <CollapsiblePanel
              title="PCAP analysis"
              subtitle={`Controller-side aggregate analysis from ${
                analysis.datalink_name || "PCAP"
              } data.`}
              badge={<span className="status-badge status-good">ready</span>}
              defaultOpen
            >
              <section className="analysis-overview-grid">
                <div className="capture-summary-card">
                  <span>Analyzed packets</span>
                  <strong>{analysis.packet_count}</strong>
                  <p>Total packets read from controller PCAP.</p>
                </div>

                <div className="capture-summary-card">
                  <span>Analyzed bytes</span>
                  <strong>{formatBytes(analysis.byte_count)}</strong>
                  <p>Bytes reported by PCAP packet headers.</p>
                </div>

                <div className="capture-summary-card">
                  <span>Datalink</span>
                  <strong>{analysis.datalink_name || "unknown"}</strong>
                  <p>Detected capture link-layer type.</p>
                </div>

                <div className="capture-summary-card">
                  <span>PCAP duration</span>
                  <strong>
                    {formatDurationSeconds(
                      Math.max(0, Math.round(analysis.duration_seconds)),
                    )}
                  </strong>
                  <p>
                    {formatUnixTime(analysis.first_packet_time)} →{" "}
                    {formatUnixTime(analysis.last_packet_time)}
                  </p>
                </div>
              </section>

              <div className="protocol-card-grid">
                <ProtocolCard label="Ethernet" value={analysis.protocols.ethernet} />
                <ProtocolCard label="ARP" value={analysis.protocols.arp} />
                <ProtocolCard label="IPv4" value={analysis.protocols.ipv4} />
                <ProtocolCard label="IPv6" value={analysis.protocols.ipv6} />
                <ProtocolCard label="TCP" value={analysis.protocols.tcp} />
                <ProtocolCard label="UDP" value={analysis.protocols.udp} />
                <ProtocolCard label="ICMP" value={analysis.protocols.icmp} />
                <ProtocolCard label="ICMPv6" value={analysis.protocols.icmpv6} />
                <ProtocolCard label="Other L3" value={analysis.protocols.other_l3} />
                <ProtocolCard label="Other L4" value={analysis.protocols.other_l4} />
              </div>

              <section className="analysis-grid">
                <AnalysisCounterTable
                  title="Top source IPs"
                  counters={analysis.top_source_ips}
                />

                <AnalysisCounterTable
                  title="Top destination IPs"
                  counters={analysis.top_destination_ips}
                />

                <AnalysisCounterTable
                  title="Top source ports"
                  counters={analysis.top_source_ports}
                />

                <AnalysisCounterTable
                  title="Top destination ports"
                  counters={analysis.top_destination_ports}
                />
              </section>
            </CollapsiblePanel>
          )}

          <CollapsiblePanel
            title="Capture technical details"
            subtitle="Session, agent, configuration, result, timeline, storage, and diagnostics."
            defaultOpen={false}
          >
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
                    <dd>
                      {capture.config.live_output ? "Enabled" : "Disabled"}
                    </dd>
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
                <h4>Controller storage</h4>

                {storedCapture?.exists ? (
                  <dl className="compact-detail-list">
                    <div>
                      <dt>Status</dt>
                      <dd>
                        <span className="status-badge status-good">
                          Stored on controller
                        </span>
                      </dd>
                    </div>

                    <div>
                      <dt>Local path</dt>
                      <dd>
                        <code>{storedCapture.local_path}</code>
                      </dd>
                    </div>

                    <div>
                      <dt>Stored size</dt>
                      <dd>{formatBytes(storedCapture.file_size_bytes)}</dd>
                    </div>

                    <div>
                      <dt>Fetched at</dt>
                      <dd>{formatUnixTime(storedCapture.fetched_at)}</dd>
                    </div>
                  </dl>
                ) : (
                  <div className="diagnostic-box">
                    This capture is not stored on the controller yet. Use{" "}
                    <strong>Fetch to controller</strong> after the capture
                    completes to prepare it for analysis and packet inspection.
                  </div>
                )}
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

                {!isActiveCapture(capture) && storedCapture?.exists && !analysis && (
                  <div className="future-action-box">
                    Controller-local PCAP copy is ready. Use{" "}
                    <strong>Analyze</strong> for aggregate statistics or{" "}
                    <strong>Packet view</strong> for per-packet inspection.
                  </div>
                )}

                {!isActiveCapture(capture) && storedCapture?.exists && analysis && (
                  <div className="future-action-box">
                    Analysis is available and was calculated from{" "}
                    <code>{analysis.local_path}</code>.
                  </div>
                )}

                {!isActiveCapture(capture) && !storedCapture?.exists && (
                  <div className="future-action-box">
                    PCAP can be downloaded directly through the agent proxy, or
                    fetched into controller storage for later analysis.
                  </div>
                )}
              </div>
            </section>
          </CollapsiblePanel>
        </>
      )}
    </div>
  );
}

function ProtocolCard({ label, value }: { label: string; value: number }) {
  return (
    <div className="protocol-analysis-card">
      <span>{label}</span>
      <strong>{value}</strong>
    </div>
  );
}

function AnalysisCounterTable({
  title,
  counters,
}: {
  title: string;
  counters: PcapAnalysisCounter[];
}) {
  return (
    <div className="analysis-card">
      <h4>{title}</h4>

      {counters.length === 0 ? (
        <p className="muted-text">No values found.</p>
      ) : (
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>Value</th>
                <th>Packets</th>
                <th>Bytes</th>
              </tr>
            </thead>

            <tbody>
              {counters.map((counter) => (
                <tr key={counter.key}>
                  <td>
                    <code>{counter.key}</code>
                  </td>
                  <td>{counter.packets}</td>
                  <td>{formatBytes(counter.bytes)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}