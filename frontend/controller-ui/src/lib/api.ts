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