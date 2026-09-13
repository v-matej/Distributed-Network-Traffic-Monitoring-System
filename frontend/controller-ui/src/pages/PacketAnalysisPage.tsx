import { useEffect, useMemo, useState } from "react";
import { Link, useParams } from "react-router-dom";

import {
  getControllerStoredCapturePacket,
  listControllerStoredCapturePackets,
} from "../lib/api";

import { formatBytes } from "../lib/format";

import type {
  PcapPacketDetail,
  PcapPacketList,
  PcapPacketSummary,
} from "../lib/api";

const DEFAULT_LIMIT = 200;

type ProtocolFilter =
  | "tcp"
  | "udp"
  | "icmp"
  | "arp"
  | "ip"
  | "ip6";

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

type DirectionFilter =
  | "any"
  | "source"
  | "destination";

const PROTOCOL_FILTERS: Array<{
  id: ProtocolFilter;
  label: string;
  className: string;
}> = [
  {
    id: "tcp",
    label: "TCP",
    className: "protocol-tcp",
  },
  {
    id: "udp",
    label: "UDP",
    className: "protocol-udp",
  },
  {
    id: "icmp",
    label: "ICMP",
    className: "protocol-icmp",
  },
  {
    id: "arp",
    label: "ARP",
    className: "protocol-arp",
  },
  {
    id: "ip",
    label: "IPv4",
    className: "protocol-ip",
  },
  {
    id: "ip6",
    label: "IPv6",
    className: "protocol-ip6",
  },
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

export function PacketAnalysisPage() {
  const { agentId, captureId } = useParams();

  const [packetList, setPacketList] =
    useState<PcapPacketList | null>(null);

  const [selectedPacket, setSelectedPacket] =
    useState<PcapPacketDetail | null>(null);

  const [selectedNumber, setSelectedNumber] =
    useState<number | null>(null);

  const [offset, setOffset] = useState(0);
  const [limit, setLimit] = useState(DEFAULT_LIMIT);

  /*
   * Graphical filter builder.
   */
  const [protocolFilters, setProtocolFilters] =
    useState<ProtocolFilter[]>([]);

  const [protocolJoinMode, setProtocolJoinMode] =
    useState<FilterJoinMode>("any");

  const [presetFilters, setPresetFilters] =
    useState<PresetFilter[]>([]);

  const [presetJoinMode, setPresetJoinMode] =
    useState<FilterJoinMode>("any");

  const [hostDirection, setHostDirection] =
    useState<DirectionFilter>("any");

  const [hostFilter, setHostFilter] =
    useState("");

  const [portDirection, setPortDirection] =
    useState<DirectionFilter>("any");

  const [portFilter, setPortFilter] =
    useState("");

  const [advancedFilter, setAdvancedFilter] =
    useState("");

  /*
   * activeFilter is the filter currently applied to
   * the packet list.
   *
   * generatedFilter can change while the user edits
   * controls, but those changes are not applied until
   * the user presses Apply filter.
   */
  const [activeFilter, setActiveFilter] =
    useState("");

  const [isLoadingList, setIsLoadingList] =
    useState(true);

  const [isLoadingPacket, setIsLoadingPacket] =
    useState(false);

  const [errorMessage, setErrorMessage] =
    useState<string | null>(null);

  const generatedFilter = useMemo(
    () =>
      buildFilterExpression({
        protocols: protocolFilters,
        protocolJoinMode,
        presets: presetFilters,
        presetJoinMode,
        hostDirection,
        hostFilter,
        portDirection,
        portFilter,
        advancedFilter,
      }),
    [
      protocolFilters,
      protocolJoinMode,
      presetFilters,
      presetJoinMode,
      hostDirection,
      hostFilter,
      portDirection,
      portFilter,
      advancedFilter,
    ],
  );

  const filterHasPendingChanges =
    generatedFilter !== activeFilter;

  const pageInfo = useMemo(() => {
    if (!packetList) {
      return "No packets loaded";
    }

    const start =
      packetList.total_packets === 0
        ? 0
        : packetList.offset + 1;

    const end = Math.min(
      packetList.offset +
        packetList.packets.length,
      packetList.total_packets,
    );

    return `${start}-${end} of ${packetList.total_packets}`;
  }, [packetList]);

  async function loadPacketList(
    nextOffset = offset,
    nextLimit = limit,
    nextFilter = activeFilter,
  ) {
    if (!agentId || !captureId) {
      return;
    }

    setIsLoadingList(true);
    setErrorMessage(null);

    try {
      const result =
        await listControllerStoredCapturePackets(
          agentId,
          captureId,
          nextOffset,
          nextLimit,
          nextFilter,
        );

      setPacketList(result.packet_list);
      setOffset(nextOffset);
      setLimit(nextLimit);

      /*
       * Select the first packet returned by the
       * current filtered page.
       *
       * Packet numbers remain the original PCAP
       * frame numbers.
       */
      if (
        result.packet_list.packets.length >
        0
      ) {
        await loadPacketDetail(
          result.packet_list.packets[0]
            .number,
        );
      } else {
        setSelectedPacket(null);
        setSelectedNumber(null);
      }
    } catch (error) {
      setErrorMessage(
        error instanceof Error
          ? error.message
          : "Failed to load packet list",
      );
    } finally {
      setIsLoadingList(false);
    }
  }

  async function loadPacketDetail(
    packetNumber: number,
  ) {
    if (!agentId || !captureId) {
      return;
    }

    setIsLoadingPacket(true);
    setErrorMessage(null);
    setSelectedNumber(packetNumber);

    try {
      const result =
        await getControllerStoredCapturePacket(
          agentId,
          captureId,
          packetNumber,
        );

      setSelectedPacket(
        result.packet,
      );
    } catch (error) {
      setErrorMessage(
        error instanceof Error
          ? error.message
          : "Failed to load packet detail",
      );
    } finally {
      setIsLoadingPacket(false);
    }
  }

  function toggleProtocolFilter(
    protocol: ProtocolFilter,
  ) {
    setProtocolFilters((current) =>
      toggleArrayValue(
        current,
        protocol,
      ),
    );
  }

  function togglePresetFilter(
    preset: PresetFilter,
  ) {
    setPresetFilters((current) =>
      toggleArrayValue(
        current,
        preset,
      ),
    );
  }

  function handleApplyFilter() {
    const portError =
      validateOptionalPort(portFilter);

    if (portError) {
      setErrorMessage(portError);
      return;
    }

    setErrorMessage(null);

    const normalizedFilter =
      generatedFilter.trim();

    setActiveFilter(
      normalizedFilter,
    );

    /*
     * Filtering changes the result set,
     * therefore pagination must restart.
     */
    void loadPacketList(
      0,
      limit,
      normalizedFilter,
    );
  }

  function handleClearFilter() {
    setProtocolFilters([]);
    setProtocolJoinMode("any");

    setPresetFilters([]);
    setPresetJoinMode("any");

    setHostDirection("any");
    setHostFilter("");

    setPortDirection("any");
    setPortFilter("");

    setAdvancedFilter("");

    setActiveFilter("");
    setErrorMessage(null);

    void loadPacketList(
      0,
      limit,
      "",
    );
  }

  function handlePreviousPage() {
    const nextOffset =
      Math.max(
        0,
        offset - limit,
      );

    void loadPacketList(
      nextOffset,
      limit,
      activeFilter,
    );
  }

  function handleNextPage() {
    if (!packetList) {
      return;
    }

    const nextOffset =
      offset + limit;

    if (
      nextOffset >=
      packetList.total_packets
    ) {
      return;
    }

    void loadPacketList(
      nextOffset,
      limit,
      activeFilter,
    );
  }

  useEffect(() => {
    /*
     * Reset filters when navigating
     * to another capture.
     */
    setProtocolFilters([]);
    setProtocolJoinMode("any");

    setPresetFilters([]);
    setPresetJoinMode("any");

    setHostDirection("any");
    setHostFilter("");

    setPortDirection("any");
    setPortFilter("");

    setAdvancedFilter("");
    setActiveFilter("");

    setOffset(0);
    setLimit(DEFAULT_LIMIT);

    setSelectedPacket(null);
    setSelectedNumber(null);

    void loadPacketList(
      0,
      DEFAULT_LIMIT,
      "",
    );

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agentId, captureId]);

  if (!agentId || !captureId) {
    return (
      <div className="page-card">
        <h2>
          Packet analysis not found
        </h2>

        <p>
          Missing agent id or capture
          id.
        </p>

        <Link
          className="text-link"
          to="/captures"
        >
          Back to captures
        </Link>
      </div>
    );
  }

  return (
    <div className="page-stack packet-analysis-page">
      <section className="page-header packet-page-header">
        <div>
          <Link
            className="text-link"
            to={`/captures/${agentId}/${captureId}`}
          >
            ← Back to capture detail
          </Link>

          <h2>
            Packet inspection
          </h2>

          <p>
            Packet list, decoded
            protocol tree, and packet
            bytes from controller stored
            PCAP{" "}
            <code>
              {captureId}
            </code>
            .
          </p>
        </div>

        <div className="capture-detail-actions packet-page-actions">
          <select
            className="packet-select"
            value={limit}
            aria-label="Packet page size"
            onChange={(event) => {
              const nextLimit =
                Number(
                  event.target.value,
                );

              void loadPacketList(
                0,
                nextLimit,
                activeFilter,
              );
            }}
          >
            <option value={100}>
              100 packets
            </option>

            <option value={200}>
              200 packets
            </option>

            <option value={500}>
              500 packets
            </option>
          </select>

          <button
            className="secondary-button"
            onClick={() =>
              void loadPacketList(
                offset,
                limit,
                activeFilter,
              )
            }
            disabled={isLoadingList}
          >
            {isLoadingList
              ? "Refreshing..."
              : "Refresh"}
          </button>
        </div>
      </section>

      {errorMessage && (
        <div className="alert alert-error">
          {errorMessage}
        </div>
      )}

      <section className="packet-filter-builder">
        <div className="packet-filter-builder-heading">
          <div>
            <span className="packet-filter-eyebrow">
              Display filter
            </span>

            <h3>
              Filter captured packets
            </h3>

            <p>
              Build a BPF filter
              graphically and apply it
              to the already stored
              PCAP capture.
            </p>
          </div>

          {activeFilter ? (
            <span className="packet-filter-status active">
              Filter active
            </span>
          ) : (
            <span className="packet-filter-status">
              Showing all packets
            </span>
          )}
        </div>

        <div className="packet-filter-sections">
          <div className="packet-filter-section">
            <div className="packet-filter-section-heading">
              <div>
                <h4>
                  Protocol
                </h4>

                <p>
                  Select one or more
                  network protocols.
                </p>
              </div>

              <FilterJoinSelector
                value={
                  protocolJoinMode
                }
                onChange={
                  setProtocolJoinMode
                }
              />
            </div>

            <div className="compact-option-grid">
              {PROTOCOL_FILTERS.map(
                (protocol) => {
                  const isActive =
                    protocolFilters.includes(
                      protocol.id,
                    );

                  return (
                    <button
                      key={
                        protocol.id
                      }
                      type="button"
                      className={`compact-filter-button ${protocol.className} ${
                        isActive
                          ? "active"
                          : ""
                      }`}
                      onClick={() =>
                        toggleProtocolFilter(
                          protocol.id,
                        )
                      }
                    >
                      {
                        protocol.label
                      }
                    </button>
                  );
                },
              )}
            </div>
          </div>

          <div className="packet-filter-section">
            <div className="packet-filter-section-heading">
              <div>
                <h4>
                  Common traffic
                </h4>

                <p>
                  Quick filters for
                  common services.
                </p>
              </div>

              <FilterJoinSelector
                value={
                  presetJoinMode
                }
                onChange={
                  setPresetJoinMode
                }
              />
            </div>

            <div className="compact-option-grid">
              {PRESET_FILTERS.map(
                (preset) => {
                  const isActive =
                    presetFilters.includes(
                      preset.id,
                    );

                  return (
                    <button
                      key={
                        preset.id
                      }
                      type="button"
                      title={
                        preset.expression
                      }
                      className={`compact-filter-button ${preset.className} ${
                        isActive
                          ? "active"
                          : ""
                      }`}
                      onClick={() =>
                        togglePresetFilter(
                          preset.id,
                        )
                      }
                    >
                      {
                        preset.label
                      }
                    </button>
                  );
                },
              )}
            </div>
          </div>

          <div className="packet-filter-fields">
            <div className="packet-filter-field-card">
              <div className="packet-filter-field-heading">
                <div>
                  <h4>
                    Host
                  </h4>

                  <p>
                    Match an IP address
                    or hostname.
                  </p>
                </div>
              </div>

              <DirectionSelector
                value={
                  hostDirection
                }
                onChange={
                  setHostDirection
                }
              />

              <input
                className="packet-filter-input"
                type="text"
                value={hostFilter}
                placeholder="e.g. 192.168.56.100"
                spellCheck={false}
                onChange={(event) =>
                  setHostFilter(
                    event.target
                      .value,
                  )
                }
                onKeyDown={(
                  event,
                ) => {
                  if (
                    event.key ===
                      "Enter" &&
                    !isLoadingList
                  ) {
                    handleApplyFilter();
                  }
                }}
              />
            </div>

            <div className="packet-filter-field-card">
              <div className="packet-filter-field-heading">
                <div>
                  <h4>
                    Port
                  </h4>

                  <p>
                    Match a source,
                    destination, or any
                    port.
                  </p>
                </div>
              </div>

              <DirectionSelector
                value={
                  portDirection
                }
                onChange={
                  setPortDirection
                }
              />

              <input
                className="packet-filter-input"
                type="number"
                min={1}
                max={65535}
                value={portFilter}
                placeholder="e.g. 443"
                onChange={(event) =>
                  setPortFilter(
                    event.target
                      .value,
                  )
                }
                onKeyDown={(
                  event,
                ) => {
                  if (
                    event.key ===
                      "Enter" &&
                    !isLoadingList
                  ) {
                    handleApplyFilter();
                  }
                }}
              />
            </div>
          </div>

          <details className="packet-filter-advanced">
            <summary>
              Advanced BPF expression
            </summary>

            <div className="packet-filter-advanced-body">
              <p>
                Optional raw BPF
                expression. It is
                combined with the
                graphical filters using
                AND.
              </p>

              <input
                className="packet-filter-input"
                type="text"
                value={
                  advancedFilter
                }
                placeholder="e.g. tcp[tcpflags] & tcp-syn != 0"
                spellCheck={false}
                onChange={(event) =>
                  setAdvancedFilter(
                    event.target
                      .value,
                  )
                }
                onKeyDown={(
                  event,
                ) => {
                  if (
                    event.key ===
                      "Enter" &&
                    !isLoadingList
                  ) {
                    handleApplyFilter();
                  }
                }}
              />
            </div>
          </details>
        </div>

        <div className="packet-filter-preview">
          <div className="packet-filter-preview-content">
            <span>
              Generated BPF
            </span>

            <code>
              {generatedFilter ||
                "No packet filter"}
            </code>
          </div>

          <div className="packet-filter-preview-actions">
            {filterHasPendingChanges && (
              <span className="packet-filter-pending">
                Changes not applied
              </span>
            )}

            <button
              type="button"
              className="small-button"
              onClick={
                handleClearFilter
              }
              disabled={
                isLoadingList &&
                !generatedFilter &&
                !activeFilter
              }
            >
              Clear
            </button>

            <button
              type="button"
              className="primary-button"
              onClick={
                handleApplyFilter
              }
              disabled={
                isLoadingList
              }
            >
              {isLoadingList
                ? "Applying..."
                : "Apply filter"}
            </button>
          </div>
        </div>

        {activeFilter && (
          <div className="packet-active-filter">
            <span>
              Active filter
            </span>

            <code>
              {activeFilter}
            </code>
          </div>
        )}
      </section>

      <section className="packet-toolbar">
        <div>
          <span>
            Range
          </span>

          <strong>
            {pageInfo}
          </strong>
        </div>

        <div>
          <span>
            Datalink
          </span>

          <strong>
            {packetList
              ?.datalink_name ||
              "unknown"}
          </strong>
        </div>

        <div>
          <span>
            Selected
          </span>

          <strong>
            {selectedNumber !== null
              ? `#${selectedNumber}`
              : "none"}
          </strong>
        </div>

        <div className="packet-pager">
          <button
            className="small-button"
            onClick={
              handlePreviousPage
            }
            disabled={
              offset === 0 ||
              isLoadingList
            }
          >
            Previous
          </button>

          <button
            className="small-button"
            onClick={
              handleNextPage
            }
            disabled={
              !packetList ||
              offset + limit >=
                packetList.total_packets ||
              isLoadingList
            }
          >
            Next
          </button>
        </div>
      </section>

      <section className="packet-list-panel">
        <div className="packet-panel-heading">
          <div>
            <h3>
              Packet list
            </h3>

            <p>
              No. · Time · Source ·
              Destination · Protocol ·
              Length · Info
            </p>
          </div>

          <span className="packet-panel-badge">
            {packetList
              ?.packets.length ??
              0}{" "}
            loaded
          </span>
        </div>

        <PacketListTable
          packets={
            packetList?.packets ??
            []
          }
          selectedNumber={
            selectedNumber
          }
          onSelect={(packet) =>
            void loadPacketDetail(
              packet.number,
            )
          }
          isLoading={
            isLoadingList
          }
          isFiltered={
            activeFilter.length >
            0
          }
        />
      </section>

      <section className="packet-lower-grid">
        <div className="packet-detail-panel">
          <div className="packet-panel-heading">
            <div>
              <h3>
                Frame and packet
                details
              </h3>

              <p>
                Decoded protocol layers
                for the selected
                packet.
              </p>
            </div>

            {selectedPacket && (
              <span className="packet-panel-badge">
                {
                  selectedPacket
                    .layers.length
                }{" "}
                layers
              </span>
            )}
          </div>

          <PacketDetailTree
            packet={
              selectedPacket
            }
            isLoading={
              isLoadingPacket
            }
          />
        </div>

        <div className="packet-bytes-panel">
          <div className="packet-panel-heading">
            <div>
              <h3>
                Packet bytes
              </h3>

              <p>
                Hexadecimal and ASCII
                representation.
              </p>
            </div>

            {selectedPacket && (
              <span className="packet-panel-badge">
                {formatBytes(
                  selectedPacket
                    .summary
                    .captured_length,
                )}
              </span>
            )}
          </div>

          <PacketBytes
            packet={
              selectedPacket
            }
            isLoading={
              isLoadingPacket
            }
          />
        </div>
      </section>
    </div>
  );
}

function FilterJoinSelector({
  value,
  onChange,
}: {
  value: FilterJoinMode;
  onChange: (
    value: FilterJoinMode,
  ) => void;
}) {
  return (
    <div className="packet-filter-join">
      <button
        type="button"
        className={
          value === "any"
            ? "active"
            : ""
        }
        onClick={() =>
          onChange("any")
        }
      >
        Any
      </button>

      <button
        type="button"
        className={
          value === "all"
            ? "active"
            : ""
        }
        onClick={() =>
          onChange("all")
        }
      >
        All
      </button>
    </div>
  );
}

function DirectionSelector({
  value,
  onChange,
}: {
  value: DirectionFilter;
  onChange: (
    value: DirectionFilter,
  ) => void;
}) {
  return (
    <div className="packet-filter-direction">
      <button
        type="button"
        className={
          value === "any"
            ? "active"
            : ""
        }
        onClick={() =>
          onChange("any")
        }
      >
        Any
      </button>

      <button
        type="button"
        className={
          value === "source"
            ? "active"
            : ""
        }
        onClick={() =>
          onChange("source")
        }
      >
        Source
      </button>

      <button
        type="button"
        className={
          value ===
          "destination"
            ? "active"
            : ""
        }
        onClick={() =>
          onChange(
            "destination",
          )
        }
      >
        Destination
      </button>
    </div>
  );
}

function PacketListTable({
  packets,
  selectedNumber,
  onSelect,
  isLoading,
  isFiltered,
}: {
  packets: PcapPacketSummary[];
  selectedNumber:
    | number
    | null;
  onSelect: (
    packet: PcapPacketSummary,
  ) => void;
  isLoading: boolean;
  isFiltered: boolean;
}) {
  if (
    isLoading &&
    packets.length === 0
  ) {
    return (
      <div className="packet-pane-empty">
        <h3>
          Loading packets...
        </h3>

        <p>
          Reading packet summaries
          from controller-local PCAP
          storage.
        </p>
      </div>
    );
  }

  if (packets.length === 0) {
    return (
      <div className="packet-pane-empty">
        <h3>
          {isFiltered
            ? "No matching packets"
            : "No packets"}
        </h3>

        <p>
          {isFiltered
            ? "No packets in this capture match the active display filter."
            : "No packet entries were returned for this capture."}
        </p>
      </div>
    );
  }

  return (
    <div className="packet-table-wrap">
      <table className="packet-table">
        <thead>
          <tr>
            <th>No.</th>
            <th>Time</th>
            <th>Source</th>
            <th>Destination</th>
            <th>Protocol</th>
            <th>Length</th>
            <th>Info</th>
          </tr>
        </thead>

        <tbody>
          {packets.map(
            (packet) => (
              <tr
                key={
                  packet.number
                }
                className={
                  packet.number ===
                  selectedNumber
                    ? "selected"
                    : ""
                }
                onClick={() =>
                  onSelect(
                    packet,
                  )
                }
              >
                <td>
                  {
                    packet.number
                  }
                </td>

                <td>
                  {packet.relative_time_seconds.toFixed(
                    6,
                  )}
                </td>

                <td>
                  <code>
                    {
                      packet.source
                    }
                  </code>
                </td>

                <td>
                  <code>
                    {
                      packet.destination
                    }
                  </code>
                </td>

                <td>
                  <span
                    className={`protocol-chip protocol-${packet.protocol.toLowerCase()}`}
                  >
                    {
                      packet.protocol
                    }
                  </span>
                </td>

                <td>
                  {
                    packet.length
                  }
                </td>

                <td className="packet-info-cell">
                  {
                    packet.info
                  }
                </td>
              </tr>
            ),
          )}
        </tbody>
      </table>
    </div>
  );
}

function PacketDetailTree({
  packet,
  isLoading,
}: {
  packet:
    | PcapPacketDetail
    | null;
  isLoading: boolean;
}) {
  if (isLoading) {
    return (
      <div className="packet-pane-empty">
        <h3>
          Loading packet...
        </h3>

        <p>
          Decoding selected packet
          layers.
        </p>
      </div>
    );
  }

  if (!packet) {
    return (
      <div className="packet-pane-empty">
        <h3>
          No packet selected
        </h3>

        <p>
          Select a packet from the
          packet list.
        </p>
      </div>
    );
  }

  return (
    <div className="packet-detail-tree">
      {packet.layers.map(
        (layer) => (
          <details
            key={
              layer.name
            }
            className="packet-layer"
            open
          >
            <summary>
              {
                layer.name
              }
            </summary>

            <dl>
              {layer.fields.map(
                (field) => (
                  <div
                    key={`${layer.name}-${field.name}`}
                  >
                    <dt>
                      {
                        field.name
                      }
                    </dt>

                    <dd>
                      {
                        field.value
                      }
                    </dd>
                  </div>
                ),
              )}
            </dl>
          </details>
        ),
      )}
    </div>
  );
}

function PacketBytes({
  packet,
  isLoading,
}: {
  packet:
    | PcapPacketDetail
    | null;
  isLoading: boolean;
}) {
  if (isLoading) {
    return (
      <div className="packet-pane-empty">
        <h3>
          Loading bytes...
        </h3>

        <p>
          Preparing packet hex dump.
        </p>
      </div>
    );
  }

  if (!packet) {
    return (
      <div className="packet-pane-empty">
        <h3>
          No bytes
        </h3>

        <p>
          Select a packet to show raw
          bytes.
        </p>
      </div>
    );
  }

  return (
    <div className="packet-bytes">
      <div className="packet-bytes-header">
        <span>
          Offset
        </span>

        <span>
          Hex
        </span>

        <span>
          ASCII
        </span>
      </div>

      {packet.hex_lines.map(
        (line) => (
          <div
            className="packet-byte-line"
            key={
              line.offset
            }
          >
            <code className="packet-byte-offset">
              {line.offset
                .toString(16)
                .padStart(
                  4,
                  "0",
                )}
            </code>

            <code className="packet-byte-hex">
              {line.hex}
            </code>

            <code className="packet-byte-ascii">
              {
                line.ascii
              }
            </code>
          </div>
        ),
      )}

      <div className="packet-byte-footer">
        Frame{" "}
        {
          packet.summary
            .number
        }{" "}
        ·{" "}
        {formatBytes(
          packet.summary
            .captured_length,
        )}{" "}
        captured
      </div>
    </div>
  );
}

function toggleArrayValue<T>(
  values: T[],
  value: T,
): T[] {
  if (
    values.includes(value)
  ) {
    return values.filter(
      (item) =>
        item !== value,
    );
  }

  return [
    ...values,
    value,
  ];
}

function buildFilterExpression({
  protocols,
  protocolJoinMode,
  presets,
  presetJoinMode,
  hostDirection,
  hostFilter,
  portDirection,
  portFilter,
  advancedFilter,
}: {
  protocols: ProtocolFilter[];
  protocolJoinMode: FilterJoinMode;

  presets: PresetFilter[];
  presetJoinMode: FilterJoinMode;

  hostDirection: DirectionFilter;
  hostFilter: string;

  portDirection: DirectionFilter;
  portFilter: string;

  advancedFilter: string;
}): string {
  const expressions: string[] = [];

  if (
    protocols.length > 0
  ) {
    const separator =
      protocolJoinMode ===
      "all"
        ? " and "
        : " or ";

    const expression =
      protocols.join(
        separator,
      );

    expressions.push(
      protocols.length > 1
        ? `(${expression})`
        : expression,
    );
  }

  if (presets.length > 0) {
    const separator =
      presetJoinMode ===
      "all"
        ? " and "
        : " or ";

    const presetExpressions =
      presets.map(
        (presetId) => {
          const preset =
            PRESET_FILTERS.find(
              (item) =>
                item.id ===
                presetId,
            );

          return (
            preset?.expression ??
            ""
          );
        },
      ).filter(Boolean);

    if (
      presetExpressions.length >
      0
    ) {
      const expression =
        presetExpressions.join(
          separator,
        );

      expressions.push(
        presetExpressions.length >
          1
          ? `(${expression})`
          : expression,
      );
    }
  }

  const normalizedHost =
    hostFilter.trim();

  if (normalizedHost) {
    if (
      hostDirection ===
      "source"
    ) {
      expressions.push(
        `src host ${normalizedHost}`,
      );
    } else if (
      hostDirection ===
      "destination"
    ) {
      expressions.push(
        `dst host ${normalizedHost}`,
      );
    } else {
      expressions.push(
        `host ${normalizedHost}`,
      );
    }
  }

  const normalizedPort =
    portFilter.trim();

  if (normalizedPort) {
    if (
      portDirection ===
      "source"
    ) {
      expressions.push(
        `src port ${normalizedPort}`,
      );
    } else if (
      portDirection ===
      "destination"
    ) {
      expressions.push(
        `dst port ${normalizedPort}`,
      );
    } else {
      expressions.push(
        `port ${normalizedPort}`,
      );
    }
  }

  const normalizedAdvanced =
    advancedFilter.trim();

  if (normalizedAdvanced) {
    expressions.push(
      `(${normalizedAdvanced})`,
    );
  }

  return expressions.join(
    " and ",
  );
}

function validateOptionalPort(
  value: string,
): string | null {
  const normalized =
    value.trim();

  if (!normalized) {
    return null;
  }

  const parsed =
    Number(normalized);

  if (
    !Number.isInteger(
      parsed,
    ) ||
    parsed < 1 ||
    parsed > 65535
  ) {
    return "Port must be an integer between 1 and 65535.";
  }

  return null;
}