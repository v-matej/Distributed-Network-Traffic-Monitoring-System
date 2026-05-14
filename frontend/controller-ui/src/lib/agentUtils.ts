import type { KnownAgent } from "./api";

export function getAgentDisplayName(agent: KnownAgent) {
  return agent.display_name || agent.agent_id;
}

export function isHealthyStatus(status: string) {
  const normalized = status.toLowerCase();

  return (
    normalized === "ok" ||
    normalized === "healthy" ||
    normalized === "running"
  );
}

export function statusClass(status: string) {
  const normalized = status.toLowerCase();

  if (isHealthyStatus(normalized)) {
    return "status-good";
  }

  if (normalized === "warning") {
    return "status-warning";
  }

  return "status-neutral";
}