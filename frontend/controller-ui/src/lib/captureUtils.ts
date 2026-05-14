import type { RemoteCaptureSessionInfo } from "./api";

import { formatBytes } from "./format";

export type CaptureDerivedStats = {
  durationSeconds: number | null;
  averageThroughput: string;
  packetRate: string;
  averagePacketSize: string;
};

export function isActiveCapture(capture: RemoteCaptureSessionInfo) {
  const status = capture.status.toLowerCase();

  return (
    status === "pending" ||
    status === "starting" ||
    status === "running" ||
    status === "stopping"
  );
}

export function isStoppableCapture(capture: RemoteCaptureSessionInfo) {
  const status = capture.status.toLowerCase();
  return status === "pending" || status === "starting" || status === "running";
}

export function isCompletedCapture(capture: RemoteCaptureSessionInfo) {
  const status = capture.status.toLowerCase();
  return status === "completed" || status === "stopped";
}

export function isFailedCapture(capture: RemoteCaptureSessionInfo) {
  return capture.status.toLowerCase() === "failed";
}

export function captureStatusClass(status: string) {
  const normalized = status.toLowerCase();

  if (normalized === "completed" || normalized === "stopped") {
    return "status-good";
  }

  if (
    normalized === "pending" ||
    normalized === "running" ||
    normalized === "starting"
  ) {
    return "status-active";
  }

  if (normalized === "stopping" || normalized === "finalizing") {
    return "status-warning";
  }

  if (normalized === "failed") {
    return "status-danger";
  }

  return "status-neutral";
}

export function getCaptureDurationSeconds(capture: RemoteCaptureSessionInfo) {
  const resultStart = capture.result.start_time;
  const resultEnd = capture.result.end_time;

  if (resultStart > 0 && resultEnd > resultStart) {
    return resultEnd - resultStart;
  }

  if (capture.started_at > 0 && capture.finished_at > capture.started_at) {
    return capture.finished_at - capture.started_at;
  }

  return null;
}

export function getCaptureDerivedStats(
  capture: RemoteCaptureSessionInfo,
): CaptureDerivedStats {
  const durationSeconds = getCaptureDurationSeconds(capture);

  if (!durationSeconds || durationSeconds <= 0) {
    return {
      durationSeconds: null,
      averageThroughput: "—",
      packetRate: "—",
      averagePacketSize:
        capture.result.packets_captured > 0
          ? formatBytes(
              capture.result.bytes_captured / capture.result.packets_captured,
            )
          : "—",
    };
  }

  const bytesPerSecond = capture.result.bytes_captured / durationSeconds;
  const packetsPerSecond = capture.result.packets_captured / durationSeconds;

  return {
    durationSeconds,
    averageThroughput: `${formatBytes(bytesPerSecond)}/s`,
    packetRate: `${packetsPerSecond.toFixed(2)} pps`,
    averagePacketSize:
      capture.result.packets_captured > 0
        ? formatBytes(
            capture.result.bytes_captured / capture.result.packets_captured,
          )
        : "—",
  };
}