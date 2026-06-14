import { useEffect, useState } from "react";
import type { FormEvent } from "react";
import { Link, useParams } from "react-router-dom";
import {
  IconActivityHeartbeat,
  IconArrowRight,
  IconDatabase,
  IconFilter,
  IconNetwork,
  IconPlayerPlay,
  IconRefresh,
  IconRouter,
  IconServer2,
  IconTerminal2,
} from "@tabler/icons-react";

import {
  captureStatusClass,
  isActiveCapture,
  isStoppableCapture,
} from "../lib/captureUtils";

import { statusClass } from "../lib/agentUtils";

import { formatBytes, formatDurationSeconds, formatUnixTime } from "../lib/format";

import {
  deleteControllerStoredCapture,
  getControllerStoredCaptureDownloadUrl,
  getAgent,
  getAgentHealth,
  getAgentInterfaces,
  listAgentCaptures,
  listControllerStoredCaptures,
  startAgentCapture,
  stopAgentCapture,
} from "../lib/api";

import type {
  AgentCapturesResponse,
  AgentInterfacesResponse,
  ControllerStoredCaptureInfo,
  KnownAgent,
  KnownAgentWithHealth,
  RemoteCaptureSessionInfo,
} from "../lib/api";

type ProtocolFilter = "tcp" | "udp" | "icmp" | "arp" | "ip" | "ip6";

type PresetFilter =
  | "dns"
  | "http"
  | "https"
  | "ssh"
  | "dhcp"
  | "ntp"
  | "mdns"
  | "ping";

type FilterJoinMode = "any" | "all";

const PROTOCOL_FILTERS: Array<{
  id: ProtocolFilter;
  label: string;
  className: string;
}> = [
  { id: "tcp", label: "TCP", className: "protocol-tcp" },
  { id: "udp", label: "UDP", className: "protocol-udp" },
  { id: "icmp", label: "ICMP", className: "protocol-icmp" },
  { id: "arp", label: "ARP", className: "protocol-arp" },
  { id: "ip", label: "IPv4", className: "protocol-ip" },
  { id: "ip6", label: "IPv6", className: "protocol-ip6" },
];

const PRESET_FILTERS: Array<{
  id: PresetFilter;
  label: string;
  expression: string;
  className: string;
}> = [
  {
    id: "dns",
    label: "DNS",
    expression: "(udp port 53 or tcp port 53)",
    className: "preset-dns",
  },
  {
    id: "http",
    label: "HTTP",
    expression: "tcp port 80",
    className: "preset-http",
  },
  {
    id: "https",
    label: "HTTPS",
    expression: "tcp port 443",
    className: "preset-https",
  },
  {
    id: "ssh",
    label: "SSH",
    expression: "tcp port 22",
    className: "preset-ssh",
  },
  {
    id: "dhcp",
    label: "DHCP",
    expression: "(udp port 67 or udp port 68)",
    className: "preset-dhcp",
  },
  {
    id: "ntp",
    label: "NTP",
    expression: "udp port 123",
    className: "preset-ntp",
  },
  {
    id: "mdns",
    label: "mDNS",
    expression: "udp port 5353",
    className: "preset-mdns",
  },
  {
    id: "ping",
    label: "Ping",
    expression: "icmp",
    className: "preset-ping",
  },
];

export function AgentDetailPage() {
  const { agentId } = useParams();

  const [agent, setAgent] = useState<KnownAgent | null>(null);
  const [health, setHealth] = useState<KnownAgentWithHealth | null>(null);
  const [interfaces, setInterfaces] = useState<AgentInterfacesResponse | null>(
    null,
  );
  const [captures, setCaptures] = useState<AgentCapturesResponse | null>(null);
  const [storedCaptures, setStoredCaptures] = useState<
    ControllerStoredCaptureInfo[]
  >([]);

  const [isLoading, setIsLoading] = useState(true);
  const [isRefreshingHealth, setIsRefreshingHealth] = useState(false);
  const [isStartingCapture, setIsStartingCapture] = useState(false);
  const [isLoadingCaptures, setIsLoadingCaptures] = useState(false);
  const [isDeletingStoredCaptureKey, setIsDeletingStoredCaptureKey] =
    useState<string | null>(null);

  const [errorMessage, setErrorMessage] = useState<string | null>(null);
  const [healthErrorMessage, setHealthErrorMessage] = useState<string | null>(
    null,
  );
  const [interfacesErrorMessage, setInterfacesErrorMessage] = useState<
    string | null
  >(null);
  const [capturesErrorMessage, setCapturesErrorMessage] = useState<
    string | null
  >(null);
  const [storedCapturesErrorMessage, setStoredCapturesErrorMessage] = useState<
    string | null
  >(null);
  const [captureMessage, setCaptureMessage] = useState<string | null>(null);
  const [lastHealthCheckAt, setLastHealthCheckAt] = useState<Date | null>(null);

  const [selectedInterface, setSelectedInterface] = useState("");

  const [protocolFilters, setProtocolFilters] = useState<ProtocolFilter[]>([]);
  const [protocolJoinMode, setProtocolJoinMode] =
    useState<FilterJoinMode>("any");

  const [presetFilters, setPresetFilters] = useState<PresetFilter[]>([]);
  const [presetJoinMode, setPresetJoinMode] = useState<FilterJoinMode>("any");

  const [sourceHost, setSourceHost] = useState("");
  const [destinationHost, setDestinationHost] = useState("");
  const [networkFilter, setNetworkFilter] = useState("");

  const [sourcePort, setSourcePort] = useState("");
  const [destinationPort, setDestinationPort] = useState("");
  const [portRange, setPortRange] = useState("");

  const [sourceMac, setSourceMac] = useState("");
  const [destinationMac, setDestinationMac] = useState("");

  const [advancedFilter, setAdvancedFilter] = useState("");

  const [enableDurationLimit, setEnableDurationLimit] = useState(true);
  const [durationSeconds, setDurationSeconds] = useState("10");

  const [enablePacketLimit, setEnablePacketLimit] = useState(false);
  const [packetCount, setPacketCount] = useState("");

  const [nowMs, setNowMs] = useState(Date.now());
  const [activeCaptureSeenAtMs, setActiveCaptureSeenAtMs] = useState<
    Record<string, number>
  >({});

  const generatedFilter = buildFilterExpression({
    protocols: protocolFilters,
    protocolJoinMode,
    presets: presetFilters,
    presetJoinMode,
    sourceHost,
    destinationHost,
    networkFilter,
    sourcePort,
    destinationPort,
    portRange,
    sourceMac,
    destinationMac,
    advancedFilter,
  });

  const activeCaptures = captures?.captures.filter(isActiveCapture) ?? [];
  const hasActiveCaptures = activeCaptures.length > 0;

  const totalCaptures = captures?.captures.length ?? 0;
  const remoteCaptureIds = new Set(
    captures?.captures.map((capture) => capture.capture_id) ?? [],
  );
  const storedOnlyCaptureCount = storedCaptures.filter(
    (capture) => !remoteCaptureIds.has(capture.capture_id),
  ).length;
  const knownCaptureCount = totalCaptures + storedOnlyCaptureCount;
  const storedCaptureCount = storedCaptures.length;

  const completedCaptures =
    captures?.captures.filter((capture) => {
      const status = capture.status.toLowerCase();
      return status === "completed" || status === "stopped";
    }).length ?? 0;

  const failedCaptures =
    captures?.captures.filter(
      (capture) => capture.status.toLowerCase() === "failed",
    ).length ?? 0;

  const interfaceCount = interfaces?.interfaces.length ?? 0;

  const endpointLabel = agent
    ? `${agent.host}:${agent.port}`
    : "Loading agent endpoint...";

  const healthStatus = health
    ? health.health.status
    : healthErrorMessage
      ? "unavailable"
      : "unknown";

  async function loadStoredCapturesForAgent(
    options: { silent?: boolean } = {},
  ) {
    if (!agentId) {
      return;
    }

    if (!options.silent) {
      setStoredCapturesErrorMessage(null);
    }

    try {
      const result = await listControllerStoredCaptures();
      setStoredCaptures(
        result.filter((storedCapture) => storedCapture.agent_id === agentId),
      );
      setStoredCapturesErrorMessage(null);
    } catch (error) {
      setStoredCaptures([]);

      if (!options.silent) {
        setStoredCapturesErrorMessage(
          getErrorMessage(error, "Stored captures unavailable"),
        );
      }
    }
  }

  async function loadAgentDetail() {
    if (!agentId) {
      setErrorMessage("Missing agent id.");
      setIsLoading(false);
      return;
    }

    setIsLoading(true);
    setErrorMessage(null);
    setHealthErrorMessage(null);
    setInterfacesErrorMessage(null);
    setCapturesErrorMessage(null);
    setStoredCapturesErrorMessage(null);

    const [
      agentResult,
      healthResult,
      interfacesResult,
      capturesResult,
      storedCapturesResult,
    ] = await Promise.allSettled([
      getAgent(agentId),
      getAgentHealth(agentId),
      getAgentInterfaces(agentId),
      listAgentCaptures(agentId),
      listControllerStoredCaptures(),
    ]);

    if (agentResult.status === "fulfilled") {
      setAgent(agentResult.value);
    } else {
      setAgent(null);
      setErrorMessage(
        `Agent metadata unavailable: ${getErrorMessage(
          agentResult.reason,
          "Failed to load agent metadata",
        )}`,
      );
    }

    if (healthResult.status === "fulfilled") {
      setHealth(healthResult.value);
      setLastHealthCheckAt(new Date());
      setHealthErrorMessage(null);
    } else {
      setHealth(null);
      setLastHealthCheckAt(new Date());
      setHealthErrorMessage(
        getErrorMessage(healthResult.reason, "Health check failed"),
      );
    }

    if (interfacesResult.status === "fulfilled") {
      setInterfaces(interfacesResult.value);
      setInterfacesErrorMessage(null);
      setSelectedInterface((current) => {
        const hasCurrentInterface = interfacesResult.value.interfaces.some(
          (iface) => iface.name === current,
        );

        if (hasCurrentInterface) {
          return current;
        }

        return interfacesResult.value.interfaces[0]?.name || "";
      });
    } else {
      setInterfaces(null);
      setSelectedInterface("");
      setInterfacesErrorMessage(
        getErrorMessage(interfacesResult.reason, "Interface list failed"),
      );
    }

    if (capturesResult.status === "fulfilled") {
      setCaptures(capturesResult.value);
      setCapturesErrorMessage(null);
    } else {
      setCaptures(null);
      setCapturesErrorMessage(
        getErrorMessage(capturesResult.reason, "Capture list failed"),
      );
    }

    if (storedCapturesResult.status === "fulfilled") {
      setStoredCaptures(
        storedCapturesResult.value.filter(
          (storedCapture) => storedCapture.agent_id === agentId,
        ),
      );
      setStoredCapturesErrorMessage(null);
    } else {
      setStoredCaptures([]);
      setStoredCapturesErrorMessage(
        getErrorMessage(
          storedCapturesResult.reason,
          "Stored captures unavailable",
        ),
      );
    }

    setIsLoading(false);
  }

  async function refreshHealth() {
    if (!agentId) {
      return;
    }

    setIsRefreshingHealth(true);
    setHealthErrorMessage(null);

    try {
      const healthResult = await getAgentHealth(agentId);
      setHealth(healthResult);
      setLastHealthCheckAt(new Date());
      setHealthErrorMessage(null);
    } catch (error) {
      setHealth(null);
      setLastHealthCheckAt(new Date());
      setHealthErrorMessage(
        getErrorMessage(error, "Failed to refresh health"),
      );
    } finally {
      setIsRefreshingHealth(false);
    }
  }

  async function loadCaptures(options: { silent?: boolean } = {}) {
    if (!agentId) {
      return;
    }

    if (!options.silent) {
      setIsLoadingCaptures(true);
      setErrorMessage(null);
      setCapturesErrorMessage(null);
    }

    try {
      const result = await listAgentCaptures(agentId);
      setCaptures(result);
      setCapturesErrorMessage(null);
      void loadStoredCapturesForAgent({ silent: true });
    } catch (error) {
      if (!options.silent) {
        setCaptures(null);
        setCapturesErrorMessage(
          getErrorMessage(error, "Failed to load captures"),
        );
      }
    } finally {
      if (!options.silent) {
        setIsLoadingCaptures(false);
      }
    }
  }

  function toggleProtocolFilter(protocol: ProtocolFilter) {
    setProtocolFilters((current) => toggleArrayValue(current, protocol));
  }

  function togglePresetFilter(preset: PresetFilter) {
    setPresetFilters((current) => toggleArrayValue(current, preset));
  }

  async function handleStartCapture(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();

    if (!agentId) {
      return;
    }

    if (!selectedInterface) {
      setErrorMessage("Select an interface before starting a capture.");
      return;
    }

    const sourcePortError = validateOptionalPort(sourcePort, "Source port");
    if (sourcePortError) {
      setErrorMessage(sourcePortError);
      return;
    }

    const destinationPortError = validateOptionalPort(
      destinationPort,
      "Destination port",
    );
    if (destinationPortError) {
      setErrorMessage(destinationPortError);
      return;
    }

    const portRangeError = validateOptionalPortRange(portRange);
    if (portRangeError) {
      setErrorMessage(portRangeError);
      return;
    }

    const parsedDuration = enableDurationLimit ? Number(durationSeconds) : 0;

    if (
      enableDurationLimit &&
      (durationSeconds.trim() === "" ||
        !Number.isInteger(parsedDuration) ||
        parsedDuration <= 0)
    ) {
      setErrorMessage("Duration must be a positive integer when enabled.");
      return;
    }

    const parsedPacketCount = enablePacketLimit ? Number(packetCount) : 0;

    if (
      enablePacketLimit &&
      (packetCount.trim() === "" ||
        !Number.isInteger(parsedPacketCount) ||
        parsedPacketCount <= 0)
    ) {
      setErrorMessage("Packet count must be a positive integer when enabled.");
      return;
    }

    setIsStartingCapture(true);
    setErrorMessage(null);
    setCaptureMessage(null);

    try {
      const request = {
        interface_name: selectedInterface,
        ...(generatedFilter ? { filter_expression: generatedFilter } : {}),
        ...(parsedDuration > 0 ? { duration_seconds: parsedDuration } : {}),
        ...(parsedPacketCount > 0 ? { packet_count: parsedPacketCount } : {}),
      };

      const result = await startAgentCapture(agentId, request);
      const firstSeenAtMs = Date.now();

      setNowMs(firstSeenAtMs);
      setActiveCaptureSeenAtMs((current) => ({
        ...current,
        [result.capture.capture_id]: firstSeenAtMs,
      }));

      setCaptures((current) => {
        if (!current) {
          return {
            agent: result.agent,
            captures: [result.capture],
          };
        }

        return {
          ...current,
          captures: upsertCapture(current.captures, result.capture),
        };
      });

      setCaptureMessage(`Capture ${result.capture.capture_id} started.`);
      void loadCaptures({ silent: true });
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to start capture",
      );
    } finally {
      setIsStartingCapture(false);
    }
  }

  async function handleStopCapture(captureId: string) {
    if (!agentId) {
      return;
    }

    setErrorMessage(null);
    setCaptureMessage(null);

    try {
      await stopAgentCapture(agentId, captureId);
      setCaptureMessage(`Stop requested for capture ${captureId}.`);
      await loadCaptures();
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to stop capture",
      );
    }
  }

  async function handleDeleteStoredCapture(
    storedCapture: ControllerStoredCaptureInfo,
  ) {
    if (!agentId) {
      return;
    }

    const key = getStoredCaptureKey(storedCapture);

    setIsDeletingStoredCaptureKey(key);
    setErrorMessage(null);
    setCaptureMessage(null);

    try {
      await deleteControllerStoredCapture(
        storedCapture.agent_id,
        storedCapture.capture_id,
      );
      setStoredCaptures((current) =>
        current.filter((item) => getStoredCaptureKey(item) !== key),
      );
      setCaptureMessage(`Deleted stored capture ${storedCapture.capture_id}.`);
      await loadCaptures({ silent: true });
    } catch (error) {
      setErrorMessage(
        error instanceof Error
          ? error.message
          : "Failed to delete stored capture",
      );
    } finally {
      setIsDeletingStoredCaptureKey(null);
    }
  }

  useEffect(() => {
    void loadAgentDetail();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agentId]);

  useEffect(() => {
    if (!agentId) {
      return;
    }

    const intervalId = window.setInterval(() => {
      void refreshHealth();
    }, 15000);

    return () => window.clearInterval(intervalId);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agentId]);

  useEffect(() => {
    if (!agentId || !hasActiveCaptures) {
      return;
    }

    const intervalId = window.setInterval(() => {
      void loadCaptures({ silent: true });
    }, 1000);

    return () => window.clearInterval(intervalId);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agentId, hasActiveCaptures]);

  useEffect(() => {
    if (!hasActiveCaptures) {
      return;
    }

    const intervalId = window.setInterval(() => {
      setNowMs(Date.now());
    }, 1000);

    return () => window.clearInterval(intervalId);
  }, [hasActiveCaptures]);

  useEffect(() => {
    const activeIds = new Set(
      activeCaptures.map((capture) => capture.capture_id),
    );
    const firstSeenAtMs = Date.now();

    setActiveCaptureSeenAtMs((current) => {
      let changed = false;
      const next = { ...current };

      for (const capture of activeCaptures) {
        if (!next[capture.capture_id]) {
          next[capture.capture_id] = firstSeenAtMs;
          changed = true;
        }
      }

      for (const captureId of Object.keys(next)) {
        if (!activeIds.has(captureId)) {
          delete next[captureId];
          changed = true;
        }
      }

      return changed ? next : current;
    });
  }, [captures]);

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
      <section className="console-hero agent-detail-hero">
        <div className="console-hero-content">
          <Link className="text-link" to="/agents">
            ← Back to agents
          </Link>

          <div className="console-kicker agent-detail-kicker">
            <span className={health ? "live-dot" : "dead-dot"} />
            Agent control surface
          </div>

          <h2>{agent?.display_name || agentId}</h2>

          <p>
            Remote capture endpoint <code>{endpointLabel}</code>. Manage health,
            interfaces, capture filters, active sessions, and capture history.
          </p>

          <div className="console-hero-meta">
            <span>
              <IconServer2 size={15} />
              {health?.health.hostname || "unknown-host"}
            </span>

            <span>
              <IconActivityHeartbeat size={15} />
              {healthStatus}
            </span>

            <span>
              <IconNetwork size={15} />
              {interfaceCount} interfaces
            </span>

            <span>
              <IconDatabase size={15} />
              {knownCaptureCount} captures
            </span>

            <span>
              <IconDatabase size={15} />
              {storedCaptureCount} stored
            </span>
          </div>
        </div>

        <div className="console-hero-actions">
          <button
            className="secondary-button"
            onClick={() => void loadAgentDetail()}
            disabled={isLoading}
          >
            <IconRefresh size={16} />
            {isLoading ? "Refreshing..." : "Refresh all"}
          </button>

          <Link className="primary-button" to="/captures">
            <IconDatabase size={16} />
            Global captures
          </Link>
        </div>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}

      {isLoading ? (
        <div className="page-card">
          <h3>Loading agent detail...</h3>
          <p>Reading agent metadata, health, interfaces, and captures.</p>
        </div>
      ) : (
        <>
          <section className="agent-summary-strip">
            <div className="agent-summary-card agent-summary-green">
              <div>
                <span>Health</span>
                <strong>{healthStatus}</strong>
              </div>
              <IconActivityHeartbeat size={20} />
            </div>

            <div className="agent-summary-card agent-summary-cyan">
              <div>
                <span>Interfaces</span>
                <strong>{interfaceCount}</strong>
              </div>
              <IconRouter size={20} />
            </div>

            <div className="agent-summary-card agent-summary-amber">
              <div>
                <span>Active captures</span>
                <strong>{activeCaptures.length}</strong>
              </div>
              <IconPlayerPlay size={20} />
            </div>

            <div className="agent-summary-card agent-summary-purple">
              <div>
                <span>Completed</span>
                <strong>{completedCaptures}</strong>
              </div>
              <IconDatabase size={20} />
            </div>

            <div className="agent-summary-card agent-summary-red">
              <div>
                <span>Failed</span>
                <strong>{failedCaptures}</strong>
              </div>
              <IconTerminal2 size={20} />
            </div>
          </section>

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
                      <span
                        className={`status-badge ${statusClass(
                          health.health.status,
                        )}`}
                      >
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
              ) : healthErrorMessage ? (
                <div className="empty-state compact-empty-state">
                  <h3>Health unavailable</h3>
                  <p>{healthErrorMessage}</p>
                </div>
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
                <h3>Start capture</h3>
                <p>Create a remote capture session on this agent.</p>
              </div>
            </div>

            {captureMessage && (
              <div className="alert alert-success">{captureMessage}</div>
            )}

            {interfacesErrorMessage && (
              <div className="alert alert-error">
                Interfaces unavailable. Capture start is disabled until the
                agent returns interface data.
              </div>
            )}

            <form className="capture-builder" onSubmit={handleStartCapture}>
              <div className="capture-builder-grid">
                <section className="capture-panel">
                  <div className="capture-panel-header">
                    <div>
                      <h4>Capture target</h4>
                      <p>Select the interface where packets should be captured.</p>
                    </div>
                  </div>

                  <label className="field-label">
                    Interface
                    <select
                      value={selectedInterface}
                      onChange={(event) =>
                        setSelectedInterface(event.target.value)
                      }
                    >
                      <option value="">Select interface</option>
                      {interfaces?.interfaces.map((iface) => (
                        <option key={iface.name} value={iface.name}>
                          {iface.name}
                          {iface.description ? ` — ${iface.description}` : ""}
                        </option>
                      ))}
                    </select>
                  </label>
                </section>

                <section className="capture-panel">
                  <div className="capture-panel-header">
                    <div>
                      <h4>Stop conditions</h4>
                      <p>Choose when the capture should finish automatically.</p>
                    </div>
                  </div>

                  <div className="stop-condition-grid">
                    <button
                      type="button"
                      className={`option-card compact-option-card ${
                        enableDurationLimit ? "active" : ""
                      }`}
                      onClick={() =>
                        setEnableDurationLimit((current) => !current)
                      }
                    >
                      <span className="option-title">Time limit</span>
                      <span className="option-description">Seconds</span>
                    </button>

                    <input
                      value={durationSeconds}
                      disabled={!enableDurationLimit}
                      inputMode="numeric"
                      placeholder="10"
                      aria-label="Duration limit in seconds"
                      title="Duration limit in seconds"
                      onChange={(event) => setDurationSeconds(event.target.value)}
                    />

                    <button
                      type="button"
                      className={`option-card compact-option-card ${
                        enablePacketLimit ? "active" : ""
                      }`}
                      onClick={() =>
                        setEnablePacketLimit((current) => !current)
                      }
                    >
                      <span className="option-title">Packet limit</span>
                      <span className="option-description">Count</span>
                    </button>

                    <input
                      value={packetCount}
                      disabled={!enablePacketLimit}
                      inputMode="numeric"
                      placeholder="100"
                      aria-label="Packet count limit"
                      title="Packet count limit"
                      onChange={(event) => setPacketCount(event.target.value)}
                    />
                  </div>
                </section>
              </div>

              <details className="advanced-filter-panel">
                <summary>
                  <div>
                    <span>Advanced filters</span>
                    <small>
                      Protocols, common traffic, IP, port, MAC, and raw BPF
                    </small>
                  </div>
                </summary>

                <div className="advanced-filter-content compact-filter-content">
                  <section className="advanced-block">
                    <div className="filter-group-heading">
                      <div>
                        <h4>Protocols</h4>
                        <p>Choose whether selected protocols should match any or all.</p>
                      </div>

                      <div className="segmented-control" aria-label="Protocol filter mode">
                        <button
                          type="button"
                          className={`segment-button ${
                            protocolJoinMode === "any" ? "active" : ""
                          }`}
                          onClick={() => setProtocolJoinMode("any")}
                        >
                          Any
                        </button>

                        <button
                          type="button"
                          className={`segment-button ${
                            protocolJoinMode === "all" ? "active" : ""
                          }`}
                          onClick={() => setProtocolJoinMode("all")}
                        >
                          All
                        </button>
                      </div>
                    </div>

                    <div className="compact-option-grid">
                      {PROTOCOL_FILTERS.map((protocol) => {
                        const isActive = protocolFilters.includes(protocol.id);

                        return (
                          <button
                            key={protocol.id}
                            type="button"
                            className={`compact-filter-button ${protocol.className} ${
                              isActive ? "active" : ""
                            }`}
                            onClick={() => toggleProtocolFilter(protocol.id)}
                          >
                            {protocol.label}
                          </button>
                        );
                      })}
                    </div>
                  </section>

                  <section className="advanced-block">
                    <div className="filter-group-heading">
                      <div>
                        <h4>Common traffic</h4>
                        <p>Combine selected presets using any or all logic.</p>
                      </div>

                      <div className="segmented-control" aria-label="Preset filter mode">
                        <button
                          type="button"
                          className={`segment-button ${
                            presetJoinMode === "any" ? "active" : ""
                          }`}
                          onClick={() => setPresetJoinMode("any")}
                        >
                          Any
                        </button>

                        <button
                          type="button"
                          className={`segment-button ${
                            presetJoinMode === "all" ? "active" : ""
                          }`}
                          onClick={() => setPresetJoinMode("all")}
                        >
                          All
                        </button>
                      </div>
                    </div>

                    <div className="compact-option-grid">
                      {PRESET_FILTERS.map((preset) => {
                        const isActive = presetFilters.includes(preset.id);

                        return (
                          <button
                            key={preset.id}
                            type="button"
                            title={preset.expression}
                            className={`compact-filter-button ${preset.className} ${
                              isActive ? "active" : ""
                            }`}
                            onClick={() => togglePresetFilter(preset.id)}
                          >
                            {preset.label}
                          </button>
                        );
                      })}
                    </div>
                  </section>

                  <section className="advanced-block">
                    <h4>IP filters</h4>

                    <div className="advanced-field-grid compact-field-grid">
                      <label className="field-label">
                        Source host
                        <input
                          value={sourceHost}
                          placeholder="192.168.56.10"
                          onChange={(event) =>
                            setSourceHost(event.target.value)
                          }
                        />
                      </label>

                      <label className="field-label">
                        Destination host
                        <input
                          value={destinationHost}
                          placeholder="8.8.8.8"
                          onChange={(event) =>
                            setDestinationHost(event.target.value)
                          }
                        />
                      </label>

                      <label className="field-label">
                        Network
                        <input
                          value={networkFilter}
                          placeholder="192.168.56.0/24"
                          onChange={(event) =>
                            setNetworkFilter(event.target.value)
                          }
                        />
                      </label>
                    </div>
                  </section>

                  <section className="advanced-block">
                    <h4>Port filters</h4>

                    <div className="advanced-field-grid compact-field-grid">
                      <label className="field-label">
                        Source port
                        <input
                          value={sourcePort}
                          inputMode="numeric"
                          placeholder="12345"
                          onChange={(event) => setSourcePort(event.target.value)}
                        />
                      </label>

                      <label className="field-label">
                        Destination port
                        <input
                          value={destinationPort}
                          inputMode="numeric"
                          placeholder="443"
                          onChange={(event) =>
                            setDestinationPort(event.target.value)
                          }
                        />
                      </label>

                      <label className="field-label">
                        Port range
                        <input
                          value={portRange}
                          placeholder="1000-2000"
                          onChange={(event) => setPortRange(event.target.value)}
                        />
                      </label>
                    </div>
                  </section>

                  <section className="advanced-block">
                    <h4>MAC filters</h4>

                    <div className="advanced-field-grid compact-field-grid">
                      <label className="field-label">
                        Source MAC
                        <input
                          value={sourceMac}
                          placeholder="aa:bb:cc:dd:ee:ff"
                          onChange={(event) => setSourceMac(event.target.value)}
                        />
                      </label>

                      <label className="field-label">
                        Destination MAC
                        <input
                          value={destinationMac}
                          placeholder="ff:ff:ff:ff:ff:ff"
                          onChange={(event) =>
                            setDestinationMac(event.target.value)
                          }
                        />
                      </label>
                    </div>
                  </section>

                  <section className="advanced-block">
                    <h4>Raw BPF</h4>

                    <label className="field-label">
                      Custom expression
                      <input
                        value={advancedFilter}
                        placeholder="src net 192.168.56.0/24 and not port 22"
                        onChange={(event) =>
                          setAdvancedFilter(event.target.value)
                        }
                      />
                    </label>
                  </section>
                </div>
              </details>

              <div className="filter-preview premium-preview bpf-preview-terminal">
                <div className="bpf-preview-header">
                  <span>
                    <IconFilter size={14} />
                    Generated capture filter
                  </span>

                  <span className="bpf-preview-mode">
                    protocols:{protocolJoinMode} · presets:{presetJoinMode}
                  </span>
                </div>

                <code>{generatedFilter || "No packet filter"}</code>
              </div>

              <div className="capture-submit-row">
                <button
                  className="primary-button"
                  type="submit"
                  disabled={isStartingCapture || !selectedInterface}
                >
                  {isStartingCapture ? "Starting capture..." : "Start capture"}
                </button>
              </div>
            </form>
          </section>

          <section className="page-card">
            <div className="section-heading">
              <div>
                <h3>Captures</h3>
                {capturesErrorMessage ? (
                  <p>Remote capture list unavailable.</p>
                ) : (
                  <p>
                    {totalCaptures} capture{totalCaptures === 1 ? "" : "s"}{" "}
                    known on this agent.
                  </p>
                )}
              </div>

              <button
                className="small-button"
                onClick={() => void loadCaptures()}
                disabled={isLoadingCaptures}
              >
                {isLoadingCaptures ? "Refreshing..." : "Refresh captures"}
              </button>
            </div>

            {capturesErrorMessage && (
              <div className="alert alert-error">
                Remote captures unavailable: {capturesErrorMessage}
              </div>
            )}

            {hasActiveCaptures && (
              <div className="active-capture-grid">
                {activeCaptures.map((capture) => {
                  const firstSeenAtMs =
                    activeCaptureSeenAtMs[capture.capture_id] ?? nowMs;
                  const visibleSeconds = getVisibleElapsedSeconds(
                    firstSeenAtMs,
                    nowMs,
                  );
                  const progressPercent = getDurationProgressPercent(
                    capture,
                    visibleSeconds,
                  );
                  const isPastDurationLimit = isCapturePastDurationLimit(
                    capture,
                    visibleSeconds,
                  );
                  const displayStatus = isPastDurationLimit
                    ? "finalizing"
                    : capture.status;

                  return (
                    <article
                      className="active-capture-card"
                      key={capture.capture_id}
                    >
                      <div className="active-capture-topline">
                        <div>
                          <span className="active-capture-label">
                            Active capture
                          </span>
                          <h4>{capture.capture_id}</h4>
                        </div>

                        <span
                          className={`status-badge ${captureStatusClass(
                            displayStatus,
                          )}`}
                        >
                          {displayStatus}
                        </span>
                      </div>

                      <div className="active-capture-stats">
                        <div className="active-capture-stat">
                          <span>UI timer</span>
                          <strong>
                            {formatDurationSeconds(
                              getDisplayedElapsedSeconds(
                                capture,
                                visibleSeconds,
                              ),
                            )}
                            {isPastDurationLimit ? "+" : ""}
                          </strong>
                        </div>

                        <div className="active-capture-stat">
                          <span>Interface</span>
                          <strong>{capture.config.interface_name || "—"}</strong>
                        </div>

                        <div className="active-capture-stat">
                          <span>Time limit</span>
                          <strong>
                            {capture.config.duration_seconds > 0
                              ? `${capture.config.duration_seconds} seconds`
                              : "No time limit"}
                          </strong>
                        </div>

                        <div className="active-capture-stat">
                          <span>Packet limit</span>
                          <strong>
                            {capture.config.packet_count > 0
                              ? `${capture.config.packet_count} packets`
                              : "No packet limit"}
                          </strong>
                        </div>
                      </div>

                      {progressPercent !== null && (
                        <div className="progress-block">
                          <div className="progress-label">
                            <span>Duration progress</span>
                            <span>{progressPercent}%</span>
                          </div>

                          <div className="progress-track">
                            <div
                              className="progress-fill"
                              style={{ width: `${progressPercent}%` }}
                            />
                          </div>
                        </div>
                      )}

                      <div className="active-capture-meta">
                        <div>
                          <span>Filter</span>
                          <code>
                            {capture.config.filter_expression ||
                              "No packet filter"}
                          </code>
                        </div>

                        <div>
                          <span>Output</span>
                          <code>{capture.config.output_file || "—"}</code>
                        </div>
                      </div>

                      <p className="active-capture-note">
                        UI timer starts when this page receives the active
                        capture. Packet and byte counters update when the
                        backend finalizes the capture.
                      </p>
                    </article>
                  );
                })}
              </div>
            )}

            {capturesErrorMessage ? (
              <div className="empty-state compact-empty-state">
                <h3>Remote captures unavailable</h3>
                <p>
                  The agent did not return its live capture list. Controller
                  stored captures remain available below.
                </p>
              </div>
            ) : !captures || captures.captures.length === 0 ? (
              <div className="empty-state">
                <h3>No captures found</h3>
                <p>Start a capture to see it listed here.</p>
              </div>
            ) : (
              <div className="table-wrap">
                <table className="data-table">
                  <thead>
                    <tr>
                      <th>ID</th>
                      <th>Status</th>
                      <th>Interface</th>
                      <th>Packets</th>
                      <th>Bytes</th>
                      <th>Stop reason</th>
                      <th>Created</th>
                      <th />
                    </tr>
                  </thead>

                  <tbody>
                    {captures.captures.map((capture) => (
                      <tr key={capture.capture_id}>
                        <td>
                          <code>{capture.capture_id}</code>
                        </td>

                        <td>
                          <span
                            className={`status-badge ${captureStatusClass(
                              capture.status,
                            )}`}
                          >
                            {capture.status}
                          </span>
                        </td>

                        <td>
                          <code>{capture.config.interface_name}</code>
                        </td>

                        <td>{capture.result.packets_captured}</td>
                        <td>{formatBytes(capture.result.bytes_captured)}</td>
                        <td>{capture.result.stop_reason || "—"}</td>
                        <td>{formatUnixTime(capture.created_at)}</td>

                        <td className="table-actions">
                          <Link
                            className="small-button"
                            to={`/captures/${agentId}/${capture.capture_id}`}
                          >
                            Open
                            <IconArrowRight size={14} />
                          </Link>

                          {isStoppableCapture(capture) && (
                            <button
                              className="small-button danger-text"
                              onClick={() =>
                                void handleStopCapture(capture.capture_id)
                              }
                            >
                              Stop
                            </button>
                          )}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </section>

          <section className="page-card">
            <div className="section-heading">
              <div>
                <h3>Controller stored captures</h3>
                <p>
                  {storedCaptureCount} persisted capture
                  {storedCaptureCount === 1 ? "" : "s"} for this agent.
                </p>
              </div>

              <button
                className="small-button"
                onClick={() => void loadStoredCapturesForAgent()}
              >
                Refresh stored
              </button>
            </div>

            {storedCapturesErrorMessage && (
              <div className="alert alert-error">
                Stored captures unavailable: {storedCapturesErrorMessage}
              </div>
            )}

            {storedCapturesErrorMessage ? (
              <div className="empty-state compact-empty-state">
                <h3>Stored captures unavailable</h3>
                <p>{storedCapturesErrorMessage}</p>
              </div>
            ) : storedCaptures.length === 0 ? (
              <div className="empty-state">
                <h3>No stored captures found</h3>
                <p>
                  Fetch a completed PCAP to controller storage to keep it
                  available while the agent is offline.
                </p>
              </div>
            ) : (
              <div className="table-wrap">
                <table className="data-table">
                  <thead>
                    <tr>
                      <th>ID</th>
                      <th>Status</th>
                      <th>Size</th>
                      <th>Fetched</th>
                      <th />
                    </tr>
                  </thead>

                  <tbody>
                    {storedCaptures.map((storedCapture) => {
                      const isDeleting =
                        isDeletingStoredCaptureKey ===
                        getStoredCaptureKey(storedCapture);

                      return (
                        <tr key={storedCapture.capture_id}>
                          <td>
                            <code>{storedCapture.capture_id}</code>
                          </td>

                          <td>
                            <span
                              className={`status-badge ${
                                storedCapture.exists
                                  ? "status-good"
                                  : "status-danger"
                              }`}
                            >
                              {storedCapture.exists ? "stored" : "missing"}
                            </span>
                          </td>

                          <td>{formatBytes(storedCapture.file_size_bytes)}</td>
                          <td>{formatUnixTime(storedCapture.fetched_at)}</td>

                          <td className="table-actions">
                            <Link
                              className="small-button"
                              to={`/captures/${agentId}/${storedCapture.capture_id}/packets`}
                            >
                              Packet view
                              <IconArrowRight size={14} />
                            </Link>

                            <a
                              className="small-button"
                              href={getControllerStoredCaptureDownloadUrl(
                                agentId,
                                storedCapture.capture_id,
                              )}
                              download
                            >
                              Download
                            </a>

                            <button
                              className="small-button danger-text"
                              disabled={isDeleting}
                              onClick={() =>
                                void handleDeleteStoredCapture(storedCapture)
                              }
                            >
                              {isDeleting ? "Deleting..." : "Delete"}
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
        </>
      )}
    </div>
  );
}

function upsertCapture(
  captures: RemoteCaptureSessionInfo[],
  capture: RemoteCaptureSessionInfo,
) {
  const exists = captures.some((item) => item.capture_id === capture.capture_id);

  if (!exists) {
    return [capture, ...captures];
  }

  return captures.map((item) =>
    item.capture_id === capture.capture_id ? capture : item,
  );
}

function toggleArrayValue<T extends string>(values: T[], value: T) {
  return values.includes(value)
    ? values.filter((item) => item !== value)
    : [...values, value];
}

function buildFilterExpression(options: {
  protocols: ProtocolFilter[];
  protocolJoinMode: FilterJoinMode;
  presets: PresetFilter[];
  presetJoinMode: FilterJoinMode;
  sourceHost: string;
  destinationHost: string;
  networkFilter: string;
  sourcePort: string;
  destinationPort: string;
  portRange: string;
  sourceMac: string;
  destinationMac: string;
  advancedFilter: string;
}) {
  const parts: string[] = [];

  const protocolExpression = joinFilterGroup(
    options.protocols,
    options.protocolJoinMode,
  );

  if (protocolExpression) {
    parts.push(protocolExpression);
  }

  const presetExpressions = options.presets
    .map((presetId) => PRESET_FILTERS.find((preset) => preset.id === presetId))
    .filter((preset): preset is (typeof PRESET_FILTERS)[number] =>
      Boolean(preset),
    )
    .map((preset) => preset.expression);

  const presetExpression = joinFilterGroup(
    presetExpressions,
    options.presetJoinMode,
  );

  if (presetExpression) {
    parts.push(presetExpression);
  }

  if (options.sourceHost.trim()) {
    parts.push(`src host ${options.sourceHost.trim()}`);
  }

  if (options.destinationHost.trim()) {
    parts.push(`dst host ${options.destinationHost.trim()}`);
  }

  if (options.networkFilter.trim()) {
    parts.push(`net ${options.networkFilter.trim()}`);
  }

  if (options.sourcePort.trim()) {
    parts.push(`src port ${options.sourcePort.trim()}`);
  }

  if (options.destinationPort.trim()) {
    parts.push(`dst port ${options.destinationPort.trim()}`);
  }

  if (options.portRange.trim()) {
    parts.push(`portrange ${options.portRange.trim()}`);
  }

  if (options.sourceMac.trim()) {
    parts.push(`ether src ${options.sourceMac.trim()}`);
  }

  if (options.destinationMac.trim()) {
    parts.push(`ether dst ${options.destinationMac.trim()}`);
  }

  if (options.advancedFilter.trim()) {
    parts.push(`(${options.advancedFilter.trim()})`);
  }

  return parts.join(" and ");
}

function joinFilterGroup(expressions: string[], mode: FilterJoinMode) {
  if (expressions.length === 0) {
    return null;
  }

  if (expressions.length === 1) {
    return expressions[0];
  }

  const operator = mode === "all" ? " and " : " or ";
  return `(${expressions.join(operator)})`;
}

function validateOptionalPort(value: string, label: string) {
  const trimmed = value.trim();

  if (!trimmed) {
    return null;
  }

  const parsed = Number(trimmed);

  if (!Number.isInteger(parsed) || parsed <= 0 || parsed > 65535) {
    return `${label} must be a number between 1 and 65535.`;
  }

  return null;
}

function validateOptionalPortRange(value: string) {
  const trimmed = value.trim();

  if (!trimmed) {
    return null;
  }

  const parts = trimmed.split("-");

  if (parts.length !== 2) {
    return "Port range must use format start-end, for example 1000-2000.";
  }

  const start = Number(parts[0]);
  const end = Number(parts[1]);

  if (
    !Number.isInteger(start) ||
    !Number.isInteger(end) ||
    start <= 0 ||
    end <= 0 ||
    start > 65535 ||
    end > 65535 ||
    start > end
  ) {
    return "Port range must be between 1 and 65535 and start must be lower than end.";
  }

  return null;
}

function getVisibleElapsedSeconds(firstSeenAtMs: number, nowMs: number) {
  return Math.max(0, Math.floor((nowMs - firstSeenAtMs) / 1000));
}

function getDurationProgressPercent(
  capture: RemoteCaptureSessionInfo,
  elapsedSeconds: number,
) {
  if (capture.config.duration_seconds <= 0) {
    return null;
  }

  return Math.min(
    100,
    Math.round((elapsedSeconds / capture.config.duration_seconds) * 100),
  );
}

function isCapturePastDurationLimit(
  capture: RemoteCaptureSessionInfo,
  elapsedSeconds: number,
) {
  if (capture.config.duration_seconds <= 0) {
    return false;
  }

  const status = capture.status.toLowerCase();
  if (status !== "pending" && status !== "starting" && status !== "running") {
    return false;
  }

  return elapsedSeconds >= capture.config.duration_seconds;
}

function getDisplayedElapsedSeconds(
  capture: RemoteCaptureSessionInfo,
  elapsedSeconds: number,
) {
  if (capture.config.duration_seconds <= 0) {
    return elapsedSeconds;
  }

  return Math.min(elapsedSeconds, capture.config.duration_seconds);
}

function getErrorMessage(error: unknown, fallback: string) {
  return error instanceof Error ? error.message : fallback;
}

function getStoredCaptureKey(storedCapture: ControllerStoredCaptureInfo) {
  return `${storedCapture.agent_id}:${storedCapture.capture_id}`;
}
