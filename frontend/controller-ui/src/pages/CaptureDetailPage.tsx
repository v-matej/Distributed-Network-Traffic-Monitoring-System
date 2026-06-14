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
  PcapConversationAnalysis,
  PcapDestinationIpAnalysis,
  PcapProtocolDistributionEntry,
  PcapAnalysisResult,
  PcapServiceAnalysis,
  RemoteCaptureSessionInfo,
} from "../lib/api";

type IpLocationLookup = {
  status: "loading" | "ready" | "error";
  country_code: string;
  country_name: string;
};

// Flip this to true for the presentation. When false, no external GeoIP API
// requests are sent and public IP locations remain unknown.
const ENABLE_PUBLIC_IP_LOCATION_LOOKUP = false;

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
  const [ipLocations, setIpLocations] = useState<Record<string, IpLocationLookup>>({});

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

  useEffect(() => {
    if (!ENABLE_PUBLIC_IP_LOCATION_LOOKUP || !analysis) {
      return;
    }

    const publicDestinationIps = getPublicDestinationIps(analysis);

    if (publicDestinationIps.length === 0) {
      return;
    }

    const abortController = new AbortController();
    let isCancelled = false;

    const missingIps = publicDestinationIps.filter((ipAddress) => {
      const cached = readIpLocationCache(ipAddress);

      if (cached) {
        setIpLocations((current) => ({
          ...current,
          [ipAddress]: cached,
        }));
        return false;
      }

      return !ipLocations[ipAddress];
    });

    if (missingIps.length === 0) {
      return () => abortController.abort();
    }

    setIpLocations((current) => {
      const next = { ...current };

      for (const ipAddress of missingIps) {
        next[ipAddress] = {
          status: "loading",
          country_code: "unknown",
          country_name: "unknown",
        };
      }

      return next;
    });

    void Promise.all(
      missingIps.map(async (ipAddress) => {
        const location = await fetchPublicIpLocation(
          ipAddress,
          abortController.signal,
        );

        if (location.status === "ready") {
          writeIpLocationCache(ipAddress, location);
        }

        return [ipAddress, location] as const;
      }),
    ).then((results) => {
      if (isCancelled) {
        return;
      }

      setIpLocations((current) => {
        const next = { ...current };

        for (const [ipAddress, location] of results) {
          next[ipAddress] = location;
        }

        return next;
      });
    });

    return () => {
      isCancelled = true;
      abortController.abort();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [analysis]);

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
              <AnalysisOverview analysis={analysis} />

              <ProtocolDistribution
                distribution={getProtocolDistribution(analysis)}
              />

              <section className="analysis-grid analysis-primary-grid">
                <ConversationTable
                  conversations={analysis.top_conversations ?? []}
                  ipLocations={ipLocations}
                />

                <DestinationServicesTable
                  services={analysis.top_destination_services ?? []}
                />

                <DestinationIpTable
                  destinations={getDestinationIpDetails(analysis)}
                  ipLocations={ipLocations}
                />
              </section>

              <details className="analysis-card advanced-analysis-counters">
                <summary>
                  <div>
                    <h4>Advanced counters</h4>
                    <p>
                      Raw counter tables kept for verification and debugging.
                    </p>
                  </div>
                </summary>

                <section className="analysis-grid">
                  <AnalysisCounterTable
                    title="Raw source IPs"
                    counters={analysis.top_source_ips}
                  />

                  <AnalysisCounterTable
                    title="Raw source ports"
                    counters={analysis.top_source_ports}
                  />

                  <AnalysisCounterTable
                    title="Raw destination ports"
                    counters={analysis.top_destination_ports}
                  />
                </section>
              </details>
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

function AnalysisOverview({ analysis }: { analysis: PcapAnalysisResult }) {
  const summary = getAnalysisSummary(analysis);
  const averagePacketSize =
    summary.average_packet_size_bytes ||
    (analysis.packet_count > 0 ? analysis.byte_count / analysis.packet_count : 0);
  const durationSeconds = Math.max(0, Math.round(analysis.duration_seconds));

  return (
    <section className="analysis-overview-grid">
      <div className="capture-summary-card">
        <span>Total packets</span>
        <strong>{analysis.packet_count}</strong>
        <p>Total packets read from controller PCAP.</p>
      </div>

      <div className="capture-summary-card">
        <span>Total bytes</span>
        <strong>{formatBytes(analysis.byte_count)}</strong>
        <p>Bytes reported by PCAP packet headers.</p>
      </div>

      <div className="capture-summary-card">
        <span>Capture duration</span>
        <strong>{formatDurationSeconds(durationSeconds)}</strong>
        <p>
          {formatUnixTime(analysis.first_packet_time)} to{" "}
          {formatUnixTime(analysis.last_packet_time)}
        </p>
      </div>

      <div className="capture-summary-card">
        <span>Average packet size</span>
        <strong>{formatBytes(averagePacketSize)}</strong>
        <p>Mean packet size across the analyzed PCAP.</p>
      </div>

      <div className="capture-summary-card">
        <span>Main protocol</span>
        <strong>{summary.main_protocol || "unknown"}</strong>
        <p>Most frequent analyzed protocol by packet count.</p>
      </div>

      <div className="capture-summary-card">
        <span>Main service</span>
        <strong>{summary.main_service || "unknown"}</strong>
        <p>Dominant destination service or port.</p>
      </div>

      <div className="capture-summary-card">
        <span>External traffic</span>
        <strong>{summary.external_traffic_detected ? "detected" : "none"}</strong>
        <p>Based on public source or destination addresses.</p>
      </div>

      <div className="capture-summary-card">
        <span>DNS traffic</span>
        <strong>{summary.dns_traffic_detected ? "detected" : "none"}</strong>
        <p>Detected through TCP/UDP port 53 usage.</p>
      </div>
    </section>
  );
}

function ProtocolDistribution({
  distribution,
}: {
  distribution: PcapProtocolDistributionEntry[];
}) {
  return (
    <section className="analysis-card analysis-wide-card">
      <h4>Protocol distribution</h4>

      {distribution.length === 0 ? (
        <p className="muted-text">No protocol distribution data found.</p>
      ) : (
        <div className="protocol-distribution-list">
          {distribution.map((entry) => (
            <div className="protocol-distribution-row" key={entry.name}>
              <div className="protocol-distribution-meta">
                <strong>{entry.name}</strong>
                <span>
                  {entry.packets} packets · {formatBytes(entry.bytes)} ·{" "}
                  {entry.percentage.toFixed(1)}%
                </span>
              </div>

              <div className="protocol-bar-track">
                <div
                  className="protocol-bar-fill"
                  style={{
                    width: `${Math.max(2, Math.min(100, entry.percentage))}%`,
                  }}
                />
              </div>
            </div>
          ))}
        </div>
      )}
    </section>
  );
}

function ConversationTable({
  conversations,
  ipLocations,
}: {
  conversations: PcapConversationAnalysis[];
  ipLocations: Record<string, IpLocationLookup>;
}) {
  return (
    <div className="analysis-card analysis-card-wide">
      <h4>Top conversations</h4>

      {conversations.length === 0 ? (
        <p className="muted-text">No transport conversations found.</p>
      ) : (
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>Source IP</th>
                <th>Destination IP</th>
                <th>Destination type</th>
                <th>Country</th>
                <th>Service</th>
                <th>Packets</th>
                <th>Bytes</th>
              </tr>
            </thead>

            <tbody>
              {conversations.map((conversation) => (
                <tr
                  key={`${conversation.source_ip}-${conversation.destination_ip}-${conversation.transport_protocol}-${conversation.service_port}`}
                >
                  <td>
                    <code>{conversation.source_ip}</code>
                  </td>
                  <td>
                    <code>{conversation.destination_ip}</code>
                  </td>
                  <td>
                    <ClassificationBadge
                      value={conversation.destination_classification}
                    />
                  </td>
                  <td>{formatConversationCountry(conversation, ipLocations)}</td>
                  <td>
                    {formatService(
                      conversation.service_name,
                      conversation.transport_protocol,
                      conversation.service_port,
                    )}
                  </td>
                  <td>{conversation.packets}</td>
                  <td>{formatBytes(conversation.bytes)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}

function DestinationServicesTable({
  services,
}: {
  services: PcapServiceAnalysis[];
}) {
  return (
    <div className="analysis-card">
      <h4>Top destination services</h4>

      {services.length === 0 ? (
        <p className="muted-text">No destination services found.</p>
      ) : (
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>Service</th>
                <th>Protocol/port</th>
                <th>Packets</th>
                <th>Bytes</th>
              </tr>
            </thead>

            <tbody>
              {services.map((service) => (
                <tr key={`${service.transport_protocol}-${service.port}`}>
                  <td>{formatServiceName(service.service_name)}</td>
                  <td>
                    <code>
                      {service.transport_protocol}/{service.port}
                    </code>
                  </td>
                  <td>{service.packets}</td>
                  <td>{formatBytes(service.bytes)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}

function DestinationIpTable({
  destinations,
  ipLocations,
}: {
  destinations: PcapDestinationIpAnalysis[];
  ipLocations: Record<string, IpLocationLookup>;
}) {
  return (
    <div className="analysis-card">
      <h4>Top destination IPs</h4>

      {destinations.length === 0 ? (
        <p className="muted-text">No destination IPs found.</p>
      ) : (
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>IP address</th>
                <th>Classification</th>
                <th>Country</th>
                <th>Packets</th>
                <th>Bytes</th>
              </tr>
            </thead>

            <tbody>
              {destinations.map((destination) => (
                <tr key={destination.ip_address}>
                  <td>
                    <code>{destination.ip_address}</code>
                  </td>
                  <td>
                    <ClassificationBadge
                      value={destination.classification}
                    />
                  </td>
                  <td>
                    {formatCountry(destination, ipLocations[destination.ip_address])}
                  </td>
                  <td>{destination.packets}</td>
                  <td>{formatBytes(destination.bytes)}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  );
}

function ClassificationBadge({
  value,
}: {
  value: string;
}) {
  return (
    <span className={`status-badge ${classificationStatusClass(value)}`}>
      {value || "unknown"}
    </span>
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

function getAnalysisSummary(analysis: PcapAnalysisResult) {
  const distribution = getProtocolDistribution(analysis);

  return {
    average_packet_size_bytes:
      analysis.summary?.average_packet_size_bytes ??
      (analysis.packet_count > 0 ? analysis.byte_count / analysis.packet_count : 0),
    main_protocol:
      analysis.summary?.main_protocol ?? distribution[0]?.name ?? "unknown",
    main_service:
      analysis.summary?.main_service ??
      getMainServiceFallback(analysis.top_destination_services ?? []),
    external_traffic_detected:
      analysis.summary?.external_traffic_detected ?? false,
    dns_traffic_detected: analysis.summary?.dns_traffic_detected ?? false,
  };
}

function getMainServiceFallback(services: PcapServiceAnalysis[]) {
  const knownService = services.find(
    (service) => service.service_name.toLowerCase() !== "unknown",
  );
  const service = knownService ?? services[0];

  if (!service) {
    return "unknown";
  }

  return formatService(
    service.service_name,
    service.transport_protocol,
    service.port,
  );
}

function getProtocolDistribution(analysis: PcapAnalysisResult) {
  if (analysis.protocol_distribution && analysis.protocol_distribution.length > 0) {
    return analysis.protocol_distribution;
  }

  const legacyValues: Array<[string, number]> = [
    ["TCP", analysis.protocols.tcp],
    ["UDP", analysis.protocols.udp],
    ["ICMP", analysis.protocols.icmp],
    ["ICMPv6", analysis.protocols.icmpv6],
    ["ARP", analysis.protocols.arp],
    ["Other L3", analysis.protocols.other_l3],
    ["Other L4", analysis.protocols.other_l4],
  ];

  const totalPackets =
    legacyValues.reduce((sum, [, value]) => sum + value, 0) ||
    analysis.packet_count;

  return legacyValues
    .filter(([, value]) => value > 0)
    .map(([name, packets]) => ({
      name,
      packets,
      bytes: 0,
      percentage:
        totalPackets > 0 ? (packets / totalPackets) * 100 : 0,
    }))
    .sort((left, right) => right.packets - left.packets);
}

function getDestinationIpDetails(analysis: PcapAnalysisResult) {
  if (
    analysis.top_destination_ip_details &&
    analysis.top_destination_ip_details.length > 0
  ) {
    return analysis.top_destination_ip_details;
  }

  return analysis.top_destination_ips.map((counter) => ({
    ip_address: counter.key,
    classification: "unknown",
    country_code: "unknown",
    country_name: "unknown",
    packets: counter.packets,
    bytes: counter.bytes,
  }));
}

function getPublicDestinationIps(analysis: PcapAnalysisResult) {
  const destinationIps = getDestinationIpDetails(analysis)
    .filter((destination) => destination.classification.toLowerCase() === "public")
    .map((destination) => destination.ip_address);

  const conversationDestinationIps = (analysis.top_conversations ?? [])
    .filter(
      (conversation) =>
        conversation.destination_classification.toLowerCase() === "public",
    )
    .map((conversation) => conversation.destination_ip);

  return [...new Set([...destinationIps, ...conversationDestinationIps])];
}

async function fetchPublicIpLocation(
  ipAddress: string,
  signal: AbortSignal,
): Promise<IpLocationLookup> {
  try {
    const response = await fetch(
      `https://ipapi.co/${encodeURIComponent(ipAddress)}/json/`,
      { signal },
    );

    if (!response.ok) {
      return makeUnknownLocation("error");
    }

    const data = (await response.json()) as {
      country_code?: unknown;
      country?: unknown;
      country_name?: unknown;
      error?: unknown;
    };

    if (data.error) {
      return makeUnknownLocation("error");
    }

    const countryCode =
      typeof data.country_code === "string"
        ? data.country_code
        : typeof data.country === "string"
          ? data.country
          : "unknown";
    const countryName =
      typeof data.country_name === "string" ? data.country_name : "unknown";

    return {
      status: countryCode.toLowerCase() === "unknown" ? "error" : "ready",
      country_code: normalizeCountryValue(countryCode),
      country_name: normalizeCountryValue(countryName),
    };
  } catch {
    if (signal.aborted) {
      return makeUnknownLocation("error");
    }

    return makeUnknownLocation("error");
  }
}

function readIpLocationCache(ipAddress: string) {
  try {
    const rawValue = window.sessionStorage.getItem(makeIpLocationCacheKey(ipAddress));

    if (!rawValue) {
      return null;
    }

    const parsed = JSON.parse(rawValue) as Partial<IpLocationLookup>;

    if (
      parsed.status !== "ready" ||
      typeof parsed.country_code !== "string" ||
      typeof parsed.country_name !== "string"
    ) {
      return null;
    }

    return {
      status: "ready",
      country_code: parsed.country_code,
      country_name: parsed.country_name,
    } satisfies IpLocationLookup;
  } catch {
    return null;
  }
}

function writeIpLocationCache(ipAddress: string, location: IpLocationLookup) {
  try {
    window.sessionStorage.setItem(
      makeIpLocationCacheKey(ipAddress),
      JSON.stringify(location),
    );
  } catch {
  }
}

function makeIpLocationCacheKey(ipAddress: string) {
  return `pktcapture.geoip.${ipAddress}`;
}

function makeUnknownLocation(status: "ready" | "error"): IpLocationLookup {
  return {
    status,
    country_code: "unknown",
    country_name: "unknown",
  };
}

function normalizeCountryValue(value: string) {
  const trimmed = value.trim();
  return trimmed || "unknown";
}

function formatService(
  serviceName: string,
  protocol: string,
  port: number,
) {
  const name = formatServiceName(serviceName);

  if (!protocol || port <= 0) {
    return name;
  }

  return `${name} (${protocol}/${port})`;
}

function formatServiceName(serviceName: string) {
  if (!serviceName || serviceName.toLowerCase() === "unknown") {
    return "Unknown service";
  }

  return serviceName;
}

function formatCountry(
  destination: PcapDestinationIpAnalysis,
  location?: IpLocationLookup,
) {
  const countryCode =
    location?.country_code && location.country_code.toLowerCase() !== "unknown"
      ? location.country_code
      : destination.country_code;
  const countryName =
    location?.country_name && location.country_name.toLowerCase() !== "unknown"
      ? location.country_name
      : destination.country_name;

  if (location?.status === "loading") {
    return "locating...";
  }

  if (!countryCode || countryCode.toLowerCase() === "unknown") {
    return "unknown";
  }

  if (!countryName || countryName.toLowerCase() === "unknown") {
    return countryCode.toUpperCase();
  }

  return `${countryCode.toUpperCase()} · ${countryName}`;
}

function formatConversationCountry(
  conversation: PcapConversationAnalysis,
  ipLocations: Record<string, IpLocationLookup>,
) {
  const location = ipLocations[conversation.destination_ip];

  return formatCountry(
    {
      ip_address: conversation.destination_ip,
      classification: conversation.destination_classification,
      country_code: conversation.destination_country_code,
      country_name: conversation.destination_country_name,
      packets: conversation.packets,
      bytes: conversation.bytes,
    },
    location,
  );
}

function classificationStatusClass(value: string) {
  switch (value.toLowerCase()) {
    case "public":
      return "status-warning";
    case "private/local":
      return "status-good";
    case "broadcast":
    case "multicast":
      return "status-active";
    case "loopback":
    case "link-local":
      return "status-neutral";
    default:
      return "status-neutral";
  }
}
