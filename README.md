# Distributed Network Traffic Monitoring System

Version: `v1.0.0`

A distributed network traffic monitoring system for Linux environments. The project is built around a central controller and multiple lightweight agents. Agents run on monitored machines, perform packet capture with `libpcap`, and expose capture operations over HTTP/JSON. The controller manages known agents, starts and stops remote captures, persists downloaded PCAP files, analyzes stored captures, and provides a React-based web interface for demonstration and inspection.

This project is intended for a diploma project scenario where the system is deployed across virtual machines on a private lab network.

---

## Implemented System

The current system includes:

- C++ packet sniffer library built on top of `libpcap`
- `packet_sniffer` CLI for low-level capture testing
- `agent_server` HTTP service for monitored machines
- `controller_server` HTTP service for central management
- persistent controller agent registry in `backend/data/known_agents.json`
- controller-side PCAP storage in `backend/data/captures/<agent_id>/`
- controller-side analysis result cache in `<capture_id>.analysis.json`
- React/Vite controller UI
- manual agent registration
- health and interface checks through the controller
- remote capture start, list, detail, stop, and download workflows
- fetch-to-controller workflow for keeping captures available when agents are offline
- packet analysis with protocol distribution, conversations, services, destination classification, and advanced counters
- packet inspection view for individual stored PCAP packets
- stored capture deletion from Agent Detail and Captures pages

Automatic local discovery is not required for v1.0.0. Agents are added manually from the UI or with the controller API.

---

## Architecture

```text
Browser
    -> React Controller UI
    -> Controller HTTP API
    -> Controller service layer
    -> Agent HTTP client
    -> Agent HTTP API
    -> AgentService
    -> AgentCaptureManager
    -> Sniffer library
    -> libpcap
```

### Main Components

**Sniffer**

- enumerates network interfaces
- captures packets from a selected interface
- supports BPF filter expressions
- supports packet-count and duration limits
- writes `.pcap` files

**Agent**

- runs on each monitored machine
- exposes `/health`, `/interfaces`, and `/captures` endpoints
- starts captures asynchronously
- tracks active and completed capture sessions
- stops running captures
- serves completed PCAP files for download

**Controller**

- stores the list of known agents
- proxies health, interface, and capture commands to agents
- stores fetched PCAP files under `backend/data/captures`
- stores capture metadata as `<capture_id>.json`
- stores cached analysis as `<capture_id>.analysis.json`
- serves stored PCAP download, analysis, packet listing, and packet detail endpoints
- allows stored captures to be deleted

**Frontend UI**

- dashboard overview
- agent registry and manual add/remove
- agent detail page with health, capture builder, remote captures, and stored captures
- global captures page
- capture detail page with analysis visualization
- packet inspection page
- settings page for controller storage actions

---

## Repository Structure

```text
.
├── Makefile
├── README.md
├── backend/
│   ├── CMakeLists.txt
│   ├── agent/
│   ├── apps/
│   │   ├── agent_server/
│   │   ├── controller_server/
│   │   └── packet_sniffer/
│   ├── controller/
│   ├── captures/
│   ├── data/
│   ├── external/
│   └── sniffer/
├── frontend/
│   └── controller-ui/
└── docs/
```

Important runtime storage:

```text
backend/data/known_agents.json
backend/data/captures/<agent_id>/<capture_id>.pcap
backend/data/captures/<agent_id>/<capture_id>.json
backend/data/captures/<agent_id>/<capture_id>.analysis.json
```

The analysis cache is valid only when the stored PCAP file size and modification time match the values saved in the cache file.

---

## Requirements

### Backend

- Linux
- CMake
- GNU Make
- C++20-compatible compiler
- `libpcap` development headers and runtime

### Frontend

- Node.js and npm

### Debian/Ubuntu Setup

```bash
sudo apt update
sudo apt install -y build-essential cmake libpcap-dev nodejs npm
```

---

## Build

Run commands from the repository root.

### Backend Build

```bash
make backend-build
```

This configures and builds:

- `backend/build/packet_sniffer`
- `backend/build/agent_server`
- `backend/build/controller_server`

### Frontend Install

```bash
make frontend-install
```

### Frontend Build

```bash
make frontend-build
```

### Clean

```bash
make clean
```

---

## Running the System

Use separate terminals for each service.

### Run Agent Server

```bash
make backend-run-agent
```

Equivalent command:

```bash
cd backend
sudo ./build/agent_server
```

The agent listens on:

```text
0.0.0.0:8080
```

Packet capture normally requires elevated privileges, so the agent is usually run with `sudo`.

### Run Controller Server

```bash
make backend-run-controller
```

Equivalent command:

```bash
cd backend
./build/controller_server
```

The controller listens on:

```text
0.0.0.0:8090
```

### Run Frontend UI

```bash
make frontend-dev
```

Equivalent command:

```bash
cd frontend/controller-ui
npm run dev -- --host 0.0.0.0
```

Open the Vite URL shown in the terminal, usually:

```text
http://127.0.0.1:5173
```

The frontend dev server proxies `/api` requests to:

```text
http://127.0.0.1:8090
```

---

## Recommended VM Topology

Use three VMs connected to the same host-only or internal network:

```text
192.168.56.0/24

controller-vm   192.168.56.10   controller_server + frontend UI
agent-1-vm      192.168.56.11   agent_server
agent-2-vm      192.168.56.12   agent_server
```

Recommended VirtualBox setup:

- Adapter 1: NAT, for package installation and updates
- Adapter 2: Host-only Adapter, for the lab network
- Host-only network: `192.168.56.0/24`

Example service placement:

```text
controller-vm:
  cd backend && ./build/controller_server
  make frontend-dev

agent-1-vm:
  cd backend && sudo ./build/agent_server

agent-2-vm:
  cd backend && sudo ./build/agent_server
```

From the host machine, open:

```text
http://192.168.56.10:5173
```

---

## Manual Agent Add

Agents can be added from the UI:

1. Open the frontend UI.
2. Go to `Agents`.
3. Enter a display name, host, and port.
4. Add each VM agent.

Example values:

```text
Display name: Agent 1
Host: 192.168.56.11
Port: 8080

Display name: Agent 2
Host: 192.168.56.12
Port: 8080
```

The same action can be performed with curl:

```bash
curl -X POST http://127.0.0.1:8090/api/agents \
  -H "Content-Type: application/json" \
  -d '{
    "display_name": "Agent 1",
    "host": "192.168.56.11",
    "port": 8080
  }'
```

The controller assigns IDs such as `agent-0001` and persists them in `backend/data/known_agents.json`.

---

## Capture Workflow

1. Open the UI.
2. Go to `Agents`.
3. Select an agent.
4. Confirm that health and interface data are available.
5. Choose an interface.
6. Optionally build a packet filter using protocol, service, host, port, MAC, or raw BPF fields.
7. Set a duration limit or packet limit.
8. Start the capture.
9. Watch active capture state in Agent Detail, Captures, or Dashboard.
10. Stop a long-running capture if needed.

Equivalent controller API request:

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures \
  -H "Content-Type: application/json" \
  -d '{
    "interface_name": "enp0s8",
    "filter_expression": "icmp or tcp port 80",
    "duration_seconds": 20
  }'
```

---

## Fetch-to-Controller Workflow

Completed PCAP files initially live on the agent that captured them. Fetching a capture copies it to controller storage so the controller can analyze it later, even if the agent is offline.

UI workflow:

1. Open a completed capture from Agent Detail or Captures.
2. Use `Fetch to controller`.
3. The capture is stored under `backend/data/captures/<agent_id>/`.
4. The stored capture appears in Agent Detail and the global Captures page.
5. Stored captures can be downloaded, analyzed, inspected packet by packet, or deleted.

Equivalent controller API request:

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID/fetch
```

Stored PCAP download:

```bash
curl -OJ http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/download
```

Delete a stored capture:

```bash
curl -X DELETE http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID
```

Deleting a stored capture removes the controller-side `.pcap`, metadata `.json`, and cached `.analysis.json` files for that capture.

---

## Analysis Workflow

Analysis runs against controller-stored PCAP files.

UI workflow:

1. Fetch a completed capture to controller storage.
2. Open the capture detail page.
3. Use the analysis section.
4. Review summary cards:
   - total packets
   - total bytes
   - capture duration
   - average packet size
   - main protocol
   - main service
   - external traffic detected
   - DNS traffic detected
5. Review protocol distribution bars.
6. Review top conversations, destination services, and destination IP classifications.
7. Expand advanced counters when raw source/destination IP and port counters are needed.

Equivalent controller API request:

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/analysis
```

When this endpoint is called:

1. The controller checks `<capture_id>.analysis.json`.
2. If file size and modification time match the PCAP, cached analysis is returned.
3. If the cache is missing or stale, the controller runs `PcapAnalyzer`.
4. A fresh analysis cache is saved and returned.

The analysis model includes:

- protocol counters and protocol distribution
- top conversations grouped by source IP, destination IP, transport protocol, and relevant service port
- top destination services with service-name mapping for common ports
- top destination IP details with private/public/broadcast/multicast/loopback/link-local classification
- optional country fields for public destination IPs
- backward-compatible raw top source IP, destination IP, source port, and destination port counters

---

## Packet Inspection Workflow

Packet inspection also runs against controller-stored PCAP files.

UI workflow:

1. Fetch a completed capture to controller storage.
2. Open `Packet view` from Agent Detail or Captures, or open the capture and navigate to packet inspection.
3. Browse packet summaries.
4. Select a packet to inspect decoded layers and hex output.

Equivalent controller API requests:

```bash
curl "http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/packets?offset=0&limit=50"
```

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/packets/1
```

---

## Useful API Endpoints

### Agent API

Base URL:

```text
http://<agent-host>:8080
```

```http
GET  /health
GET  /interfaces
POST /captures
GET  /captures
GET  /captures/{captureId}
POST /captures/{captureId}/stop
GET  /captures/{captureId}/download
```

### Controller API

Base URL:

```text
http://<controller-host>:8090
```

```http
GET    /api/agents
POST   /api/agents
DELETE /api/agents
GET    /api/agents/{agentId}
DELETE /api/agents/{agentId}
GET    /api/agents/{agentId}/health
GET    /api/agents/{agentId}/interfaces
POST   /api/agents/{agentId}/captures
GET    /api/agents/{agentId}/captures
GET    /api/agents/{agentId}/captures/{captureId}
POST   /api/agents/{agentId}/captures/{captureId}/stop
GET    /api/agents/{agentId}/captures/{captureId}/download
POST   /api/agents/{agentId}/captures/{captureId}/fetch
GET    /api/controller/captures
GET    /api/controller/captures/{agentId}/{captureId}
DELETE /api/controller/captures/{agentId}/{captureId}
GET    /api/controller/captures/{agentId}/{captureId}/download
GET    /api/controller/captures/{agentId}/{captureId}/analysis
GET    /api/controller/captures/{agentId}/{captureId}/packets
GET    /api/controller/captures/{agentId}/{captureId}/packets/{packetNumber}
```

---

## Demonstration

A complete final demo script is available in:

```text
docs/demo-scenario.md
```

The demo uses one controller VM and two agent VMs on `192.168.56.0/24`, then walks through manual agent registration, health checks, capture creation, controller-side PCAP persistence, analysis, packet inspection, offline-safe viewing, and stored capture deletion.

---

## Design Principles

- keep packet capture independent from HTTP concerns
- keep agent internals independent from controller internals
- communicate between services using HTTP/JSON
- keep controller storage readable and inspectable through JSON sidecar files
- preserve offline access to controller-stored captures
- avoid heavy frontend charting dependencies when CSS tables and bars are sufficient
- keep the system understandable for diploma demonstration and evaluation

---

## Notes

- `agent_server` and `packet_sniffer` typically require `sudo`.
- The controller can render stored captures even when an agent is offline.
- Analysis is cached per stored PCAP and invalidated by PCAP file size or modification-time changes.
- The frontend dev server is intended for the diploma/demo environment. A production deployment would serve the built frontend from a web server or from the controller.

---

## License

No license has been defined yet.
