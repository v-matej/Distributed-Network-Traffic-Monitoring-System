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

export function PacketAnalysisPage() {
  const { agentId, captureId } = useParams();

  const [packetList, setPacketList] = useState<PcapPacketList | null>(null);
  const [selectedPacket, setSelectedPacket] =
    useState<PcapPacketDetail | null>(null);
  const [selectedNumber, setSelectedNumber] = useState<number | null>(null);

  const [offset, setOffset] = useState(0);
  const [limit, setLimit] = useState(DEFAULT_LIMIT);

  const [isLoadingList, setIsLoadingList] = useState(true);
  const [isLoadingPacket, setIsLoadingPacket] = useState(false);
  const [errorMessage, setErrorMessage] = useState<string | null>(null);

  const pageInfo = useMemo(() => {
    if (!packetList) {
      return "No packets loaded";
    }

    const start = packetList.total_packets === 0 ? 0 : packetList.offset + 1;
    const end = Math.min(
      packetList.offset + packetList.packets.length,
      packetList.total_packets,
    );

    return `${start}-${end} of ${packetList.total_packets}`;
  }, [packetList]);

  async function loadPacketList(nextOffset = offset, nextLimit = limit) {
    if (!agentId || !captureId) {
      return;
    }

    setIsLoadingList(true);
    setErrorMessage(null);

    try {
      const result = await listControllerStoredCapturePackets(
        agentId,
        captureId,
        nextOffset,
        nextLimit,
      );

      setPacketList(result.packet_list);
      setOffset(nextOffset);
      setLimit(nextLimit);

      if (result.packet_list.packets.length > 0) {
        await loadPacketDetail(result.packet_list.packets[0].number);
      } else {
        setSelectedPacket(null);
        setSelectedNumber(null);
      }
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to load packet list",
      );
    } finally {
      setIsLoadingList(false);
    }
  }

  async function loadPacketDetail(packetNumber: number) {
    if (!agentId || !captureId) {
      return;
    }

    setIsLoadingPacket(true);
    setErrorMessage(null);
    setSelectedNumber(packetNumber);

    try {
      const result = await getControllerStoredCapturePacket(
        agentId,
        captureId,
        packetNumber,
      );

      setSelectedPacket(result.packet);
    } catch (error) {
      setErrorMessage(
        error instanceof Error ? error.message : "Failed to load packet detail",
      );
    } finally {
      setIsLoadingPacket(false);
    }
  }

  function handlePreviousPage() {
    const nextOffset = Math.max(0, offset - limit);
    void loadPacketList(nextOffset, limit);
  }

  function handleNextPage() {
    if (!packetList) {
      return;
    }

    const nextOffset = offset + limit;
    if (nextOffset >= packetList.total_packets) {
      return;
    }

    void loadPacketList(nextOffset, limit);
  }

  useEffect(() => {
    void loadPacketList(0, DEFAULT_LIMIT);

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [agentId, captureId]);

  if (!agentId || !captureId) {
    return (
      <div className="page-card">
        <h2>Packet analysis not found</h2>
        <p>Missing agent id or capture id.</p>

        <Link className="text-link" to="/captures">
          Back to captures
        </Link>
      </div>
    );
  }

  return (
    <div className="page-stack packet-analysis-page">
      <section className="page-header packet-page-header">
        <div>
          <Link className="text-link" to={`/captures/${agentId}/${captureId}`}>
            ← Back to capture detail
          </Link>

          <h2>Packet inspection</h2>

          <p>
            Packet list, decoded protocol tree, and packet bytes from controller
            stored PCAP <code>{captureId}</code>.
          </p>
        </div>

        <div className="capture-detail-actions packet-page-actions">
          <select
            className="packet-select"
            value={limit}
            aria-label="Packet page size"
            onChange={(event) => {
              const nextLimit = Number(event.target.value);
              void loadPacketList(0, nextLimit);
            }}
          >
            <option value={100}>100 packets</option>
            <option value={200}>200 packets</option>
            <option value={500}>500 packets</option>
          </select>

          <button
            className="secondary-button"
            onClick={() => void loadPacketList(offset, limit)}
            disabled={isLoadingList}
          >
            {isLoadingList ? "Refreshing..." : "Refresh"}
          </button>
        </div>
      </section>

      {errorMessage && <div className="alert alert-error">{errorMessage}</div>}

      <section className="packet-toolbar">
        <div>
          <span>Range</span>
          <strong>{pageInfo}</strong>
        </div>

        <div>
          <span>Datalink</span>
          <strong>{packetList?.datalink_name || "unknown"}</strong>
        </div>

        <div>
          <span>Selected</span>
          <strong>{selectedNumber ? `#${selectedNumber}` : "none"}</strong>
        </div>

        <div className="packet-pager">
          <button
            className="small-button"
            onClick={handlePreviousPage}
            disabled={offset === 0 || isLoadingList}
          >
            Previous
          </button>

          <button
            className="small-button"
            onClick={handleNextPage}
            disabled={
              !packetList ||
              offset + limit >= packetList.total_packets ||
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
            <h3>Packet list</h3>
            <p>
              No. · Time · Source · Destination · Protocol · Length · Info
            </p>
          </div>

          <span className="packet-panel-badge">
            {packetList?.packets.length ?? 0} loaded
          </span>
        </div>

        <PacketListTable
          packets={packetList?.packets ?? []}
          selectedNumber={selectedNumber}
          onSelect={(packet) => void loadPacketDetail(packet.number)}
          isLoading={isLoadingList}
        />
      </section>

      <section className="packet-lower-grid">
        <div className="packet-detail-panel">
          <div className="packet-panel-heading">
            <div>
              <h3>Frame and packet details</h3>
              <p>Decoded protocol layers for the selected packet.</p>
            </div>

            {selectedPacket && (
              <span className="packet-panel-badge">
                {selectedPacket.layers.length} layers
              </span>
            )}
          </div>

          <PacketDetailTree
            packet={selectedPacket}
            isLoading={isLoadingPacket}
          />
        </div>

        <div className="packet-bytes-panel">
          <div className="packet-panel-heading">
            <div>
              <h3>Packet bytes</h3>
              <p>Hexadecimal and ASCII representation.</p>
            </div>

            {selectedPacket && (
              <span className="packet-panel-badge">
                {formatBytes(selectedPacket.summary.captured_length)}
              </span>
            )}
          </div>

          <PacketBytes packet={selectedPacket} isLoading={isLoadingPacket} />
        </div>
      </section>
    </div>
  );
}

function PacketListTable({
  packets,
  selectedNumber,
  onSelect,
  isLoading,
}: {
  packets: PcapPacketSummary[];
  selectedNumber: number | null;
  onSelect: (packet: PcapPacketSummary) => void;
  isLoading: boolean;
}) {
  if (isLoading && packets.length === 0) {
    return (
      <div className="packet-pane-empty">
        <h3>Loading packets...</h3>
        <p>Reading packet summaries from controller-local PCAP storage.</p>
      </div>
    );
  }

  if (packets.length === 0) {
    return (
      <div className="packet-pane-empty">
        <h3>No packets</h3>
        <p>No packet entries were returned for this capture.</p>
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
          {packets.map((packet) => (
            <tr
              key={packet.number}
              className={packet.number === selectedNumber ? "selected" : ""}
              onClick={() => onSelect(packet)}
            >
              <td>{packet.number}</td>

              <td>{packet.relative_time_seconds.toFixed(6)}</td>

              <td>
                <code>{packet.source}</code>
              </td>

              <td>
                <code>{packet.destination}</code>
              </td>

              <td>
                <span
                  className={`protocol-chip protocol-${packet.protocol.toLowerCase()}`}
                >
                  {packet.protocol}
                </span>
              </td>

              <td>{packet.length}</td>

              <td className="packet-info-cell">{packet.info}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function PacketDetailTree({
  packet,
  isLoading,
}: {
  packet: PcapPacketDetail | null;
  isLoading: boolean;
}) {
  if (isLoading) {
    return (
      <div className="packet-pane-empty">
        <h3>Loading packet...</h3>
        <p>Decoding selected packet layers.</p>
      </div>
    );
  }

  if (!packet) {
    return (
      <div className="packet-pane-empty">
        <h3>No packet selected</h3>
        <p>Select a packet from the packet list.</p>
      </div>
    );
  }

  return (
    <div className="packet-detail-tree">
      {packet.layers.map((layer) => (
        <details key={layer.name} className="packet-layer" open>
          <summary>{layer.name}</summary>

          <dl>
            {layer.fields.map((field) => (
              <div key={`${layer.name}-${field.name}`}>
                <dt>{field.name}</dt>
                <dd>{field.value}</dd>
              </div>
            ))}
          </dl>
        </details>
      ))}
    </div>
  );
}

function PacketBytes({
  packet,
  isLoading,
}: {
  packet: PcapPacketDetail | null;
  isLoading: boolean;
}) {
  if (isLoading) {
    return (
      <div className="packet-pane-empty">
        <h3>Loading bytes...</h3>
        <p>Preparing packet hex dump.</p>
      </div>
    );
  }

  if (!packet) {
    return (
      <div className="packet-pane-empty">
        <h3>No bytes</h3>
        <p>Select a packet to show raw bytes.</p>
      </div>
    );
  }

  return (
    <div className="packet-bytes">
      <div className="packet-bytes-header">
        <span>Offset</span>
        <span>Hex</span>
        <span>ASCII</span>
      </div>

      {packet.hex_lines.map((line) => (
        <div className="packet-byte-line" key={line.offset}>
          <code className="packet-byte-offset">
            {line.offset.toString(16).padStart(4, "0")}
          </code>

          <code className="packet-byte-hex">{line.hex}</code>

          <code className="packet-byte-ascii">{line.ascii}</code>
        </div>
      ))}

      <div className="packet-byte-footer">
        Frame {packet.summary.number} ·{" "}
        {formatBytes(packet.summary.captured_length)} captured
      </div>
    </div>
  );
}