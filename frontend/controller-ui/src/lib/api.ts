export type KnownAgent = {
  agent_id: string;
  display_name: string;
  host: string;
  port: number;
  created_at: number;
};

export type AddAgentRequest = {
  display_name: string;
  host: string;
  port: number;
};

export type ApiError = {
  status: "error";
  message: string;
};

type AgentsResponse = {
  agents: KnownAgent[];
};

async function requestJson<T>(
  path: string,
  options: RequestInit = {},
): Promise<T> {
  const response = await fetch(path, {
    headers: {
      "Content-Type": "application/json",
      ...(options.headers ?? {}),
    },
    ...options,
  });

  const text = await response.text();
  const data = text ? JSON.parse(text) : null;

  if (!response.ok) {
    const message =
      data && typeof data.message === "string"
        ? data.message
        : `Request failed with HTTP ${response.status}`;

    throw new Error(message);
  }

  return data as T;
}

export async function listAgents(): Promise<KnownAgent[]> {
  const data = await requestJson<AgentsResponse>("/api/agents");
  return data.agents;
}

export async function addAgent(request: AddAgentRequest): Promise<KnownAgent> {
  return requestJson<KnownAgent>("/api/agents", {
    method: "POST",
    body: JSON.stringify(request),
  });
}

export async function deleteAgent(agentId: string): Promise<KnownAgent> {
  return requestJson<KnownAgent>(`/api/agents/${agentId}`, {
    method: "DELETE",
  });
}

export async function clearAgents(): Promise<void> {
  await requestJson<{ status: string; message: string }>("/api/agents", {
    method: "DELETE",
  });
}

export type RemoteHealthInfo = {
  status: string;
  agent_name: string;
  version: string;
  hostname: string;
};

export type KnownAgentWithHealth = {
  agent: KnownAgent;
  health: RemoteHealthInfo;
};

export type RemoteInterfaceInfo = {
  name: string;
  description: string;
};

export type AgentInterfacesResponse = {
  agent: KnownAgent;
  interfaces: RemoteInterfaceInfo[];
};

export async function getAgent(agentId: string): Promise<KnownAgent> {
  return requestJson<KnownAgent>(`/api/agents/${agentId}`);
}

export async function getAgentHealth(agentId: string): Promise<KnownAgentWithHealth> {
  return requestJson<KnownAgentWithHealth>(`/api/agents/${agentId}/health`);
}

export async function getAgentInterfaces(
  agentId: string,
): Promise<AgentInterfacesResponse> {
  return requestJson<AgentInterfacesResponse>(`/api/agents/${agentId}/interfaces`);
}

export type RemoteCaptureRequest = {
  interface_name: string;
  filter_expression?: string;
  packet_count?: number;
  duration_seconds?: number;
};

export type RemoteCaptureConfig = {
  interface_name: string;
  output_file: string;
  filter_expression: string;
  packet_count: number;
  duration_seconds: number;
  live_output: boolean;
};

export type RemoteCaptureResult = {
  success: boolean;
  interface_name: string;
  output_file: string;
  filter_expression: string;
  packets_captured: number;
  bytes_captured: number;
  stop_reason: string;
  start_time: number;
  end_time: number;
  error_message: string;
};

export type RemoteCaptureSessionInfo = {
  capture_id: string;
  status: string;
  stop_requested: boolean;
  config: RemoteCaptureConfig;
  result: RemoteCaptureResult;
  created_at: number;
  started_at: number;
  finished_at: number;
};

export type AgentCaptureResponse = {
  agent: KnownAgent;
  capture: RemoteCaptureSessionInfo;
};

export type AgentCapturesResponse = {
  agent: KnownAgent;
  captures: RemoteCaptureSessionInfo[];
};

export type ControllerStoredCaptureInfo = {
  agent_id: string;
  capture_id: string;
  local_path: string;
  file_size_bytes: number;
  fetched_at: number;
  exists: boolean;
};

export type ControllerStoredCaptureResponse = {
  stored_capture: ControllerStoredCaptureInfo;
};

export type ControllerStoredCapturesResponse = {
  stored_captures: ControllerStoredCaptureInfo[];
};

export async function startAgentCapture(
  agentId: string,
  request: RemoteCaptureRequest,
): Promise<AgentCaptureResponse> {
  return requestJson<AgentCaptureResponse>(`/api/agents/${agentId}/captures`, {
    method: "POST",
    body: JSON.stringify(request),
  });
}

export async function listAgentCaptures(
  agentId: string,
): Promise<AgentCapturesResponse> {
  return requestJson<AgentCapturesResponse>(`/api/agents/${agentId}/captures`);
}

export async function getAgentCapture(
  agentId: string,
  captureId: string,
): Promise<AgentCaptureResponse> {
  return requestJson<AgentCaptureResponse>(
    `/api/agents/${agentId}/captures/${captureId}`,
  );
}

export async function stopAgentCapture(
  agentId: string,
  captureId: string,
): Promise<AgentCaptureResponse> {
  return requestJson<AgentCaptureResponse>(
    `/api/agents/${agentId}/captures/${captureId}/stop`,
    {
      method: "POST",
    },
  );
}

export function getAgentCaptureDownloadUrl(agentId: string, captureId: string) {
  return `/api/agents/${agentId}/captures/${captureId}/download`;
}

export async function fetchAgentCaptureToController(
  agentId: string,
  captureId: string,
): Promise<ControllerStoredCaptureResponse> {
  return requestJson<ControllerStoredCaptureResponse>(
    `/api/agents/${agentId}/captures/${captureId}/fetch`,
    {
      method: "POST",
    },
  );
}

export async function getControllerStoredCapture(
  agentId: string,
  captureId: string,
): Promise<ControllerStoredCaptureResponse> {
  return requestJson<ControllerStoredCaptureResponse>(
    `/api/controller/captures/${agentId}/${captureId}`,
  );
}

export async function deleteControllerStoredCapture(
  agentId: string,
  captureId: string,
): Promise<ControllerStoredCaptureResponse> {
  return requestJson<ControllerStoredCaptureResponse>(
    `/api/controller/captures/${agentId}/${captureId}`,
    {
      method: "DELETE",
    },
  );
}

export async function listControllerStoredCaptures(): Promise<ControllerStoredCaptureInfo[]> {
  const data = await requestJson<ControllerStoredCapturesResponse>(
    "/api/controller/captures",
  );

  return data.stored_captures;
}

export function getControllerStoredCaptureDownloadUrl(
  agentId: string,
  captureId: string,
) {
  return `/api/controller/captures/${agentId}/${captureId}/download`;
}

export type PcapAnalysisCounter = {
  key: string;
  packets: number;
  bytes: number;
};

export type PcapProtocolCounters = {
  ethernet: number;
  arp: number;
  ipv4: number;
  ipv6: number;
  tcp: number;
  udp: number;
  icmp: number;
  icmpv6: number;
  other_l3: number;
  other_l4: number;
};

export type PcapAnalysisSummary = {
  average_packet_size_bytes: number;
  main_protocol: string;
  main_service: string;
  external_traffic_detected: boolean;
  dns_traffic_detected: boolean;
};

export type PcapProtocolDistributionEntry = {
  name: string;
  packets: number;
  bytes: number;
  percentage: number;
};

export type PcapConversationAnalysis = {
  source_ip: string;
  destination_ip: string;
  destination_classification: string;
  destination_country_code: string;
  destination_country_name: string;
  transport_protocol: string;
  service_name: string;
  service_port: number;
  packets: number;
  bytes: number;
};

export type PcapServiceAnalysis = {
  service_name: string;
  transport_protocol: string;
  port: number;
  packets: number;
  bytes: number;
};

export type PcapDestinationIpAnalysis = {
  ip_address: string;
  classification: string;
  country_code: string;
  country_name: string;
  packets: number;
  bytes: number;
};

export type PcapAnalysisResult = {
  agent_id: string;
  capture_id: string;
  local_path: string;
  datalink_name: string;
  file_size_bytes: number;
  packet_count: number;
  byte_count: number;
  analyzed_at: number;
  first_packet_time: number;
  last_packet_time: number;
  duration_seconds: number;
  protocols: PcapProtocolCounters;
  summary?: PcapAnalysisSummary;
  protocol_distribution?: PcapProtocolDistributionEntry[];
  top_conversations?: PcapConversationAnalysis[];
  top_destination_services?: PcapServiceAnalysis[];
  top_destination_ip_details?: PcapDestinationIpAnalysis[];
  top_source_ips: PcapAnalysisCounter[];
  top_destination_ips: PcapAnalysisCounter[];
  top_source_ports: PcapAnalysisCounter[];
  top_destination_ports: PcapAnalysisCounter[];
};

export type PcapAnalysisResponse = {
  analysis: PcapAnalysisResult;
};

export async function analyzeControllerStoredCapture(
  agentId: string,
  captureId: string,
): Promise<PcapAnalysisResponse> {
  return requestJson<PcapAnalysisResponse>(
    `/api/controller/captures/${agentId}/${captureId}/analysis`,
  );
}

export type PcapPacketSummary = {
  number: number;
  timestamp: number;
  timestamp_microseconds: number;
  relative_time_seconds: number;
  source: string;
  destination: string;
  protocol: string;
  length: number;
  captured_length: number;
  info: string;
};

export type PcapPacketField = {
  name: string;
  value: string;
};

export type PcapPacketLayer = {
  name: string;
  fields: PcapPacketField[];
};

export type PcapHexLine = {
  offset: number;
  hex: string;
  ascii: string;
};

export type PcapPacketDetail = {
  summary: PcapPacketSummary;
  layers: PcapPacketLayer[];
  hex_lines: PcapHexLine[];
};

export type PcapPacketList = {
  agent_id: string;
  capture_id: string;
  local_path: string;
  datalink_name: string;
  offset: number;
  limit: number;
  total_packets: number;
  packets: PcapPacketSummary[];
};

export type PcapPacketListResponse = {
  packet_list: PcapPacketList;
};

export type PcapPacketDetailResponse = {
  packet: PcapPacketDetail;
};

export async function listControllerStoredCapturePackets(
  agentId: string,
  captureId: string,
  offset = 0,
  limit = 200,
): Promise<PcapPacketListResponse> {
  return requestJson<PcapPacketListResponse>(
    `/api/controller/captures/${agentId}/${captureId}/packets?offset=${offset}&limit=${limit}`,
  );
}

export async function getControllerStoredCapturePacket(
  agentId: string,
  captureId: string,
  packetNumber: number,
): Promise<PcapPacketDetailResponse> {
  return requestJson<PcapPacketDetailResponse>(
    `/api/controller/captures/${agentId}/${captureId}/packets/${packetNumber}`,
  );
}
