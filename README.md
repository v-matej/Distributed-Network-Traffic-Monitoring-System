# Distributed Network Traffic Monitoring System

A modular distributed network traffic monitoring system built in **C++** for Linux environments.

The project currently includes:

- a **packet sniffer** built on top of **libpcap**
- an **agent** that exposes capture operations over **HTTP/JSON**
- an initial **controller backend skeleton** that can manage known agents and query them over HTTP

The long-term goal is a distributed system where multiple agents run on monitored machines and a central controller manages them through a web-based interface.

---

## Current project status

### Implemented

- packet sniffer library and CLI wrapper
- asynchronous capture management in the agent
- agent HTTP API for:
  - health
  - interfaces
  - capture creation
  - capture status
  - capture stop
- controller backend with persistent known-agent registry stored on disk
- controller HTTP API for:
  - listing known agents
  - adding agents
  - deleting one known agent
  - clearing all known-agent storage
  - querying agent health through the controller
  - querying agent interfaces through the controller
  - starting, listing, querying, and stopping remote captures through the controller

### Not yet implemented

- controller web UI
- agent auto-discovery
- PCAP download through the controller

---

## Architecture overview

### Current architecture

```text
Browser / Future Web UI
    -> Controller HTTP API
    -> Controller service layer
    -> Agent HTTP client
    -> Agent HTTP API
    -> AgentService
    -> AgentCaptureManager
    -> Sniffer library
    -> libpcap
```

### Main components

#### 1. Sniffer

Low-level packet capture logic using **libpcap**.

Responsibilities:

- enumerate interfaces
- capture packets from an interface
- support time-limited capture
- support packet-count-limited capture
- support external stop requests
- optionally save traffic to a `.pcap` file

#### 2. Agent

Runs on monitored machines and exposes packet capture functionality over HTTP.

Responsibilities:

- report health/status
- list available interfaces
- start asynchronous capture sessions
- track running and completed sessions
- stop active captures
- store generated `.pcap` files locally

#### 3. Controller

Central management component.

Current responsibilities:

- maintain a list of known agents with JSON-file persistence
- contact agents over HTTP/JSON
- expose its own HTTP API for management
- proxy agent health and interface queries
- manage remote capture lifecycle through the controller API

Planned responsibilities:

- manage multiple agents
- start and monitor remote captures
- support manual add/remove and automatic discovery
- provide a web UI

---

## Repository structure

```text
.
├── CMakeLists.txt
├── Makefile
├── README.md
├── external/
├── include/
│   ├── agent/
│   ├── controller/
│   └── sniffer/
├── src/
│   ├── agent/
│   ├── agent_app/
│   ├── cli/
│   ├── controller/
│   ├── controller_app/
│   └── sniffer/
├── captures/
├── data/
└── build/
```

### Important executables

After building, the following binaries are expected in `build/`:

- `packet_sniffer` — CLI debug/test tool for the sniffer
- `agent_server` — HTTP server exposing the agent API
- `controller_server` — HTTP server exposing the controller API

---

## Requirements

### System requirements

- Linux
- CMake
- GNU Make
- C++20-compatible compiler
- `libpcap` development headers and runtime

### Example package installation on Debian/Ubuntu

```bash
sudo apt update
sudo apt install build-essential cmake libpcap-dev
```

---

## Build

Build from the repository root:

```bash
make build
```

Clean build artifacts:

```bash
make clean
```

Manual rebuild from scratch:

```bash
rm -rf build
cmake -S . -B build
cmake --build build
```

---

## Running the components

### 1. Packet sniffer CLI

List interfaces:

```bash
sudo ./build/packet_sniffer --list
```

Capture 20 packets:

```bash
sudo ./build/packet_sniffer -i <interface> -c 20 -w test.pcap
```

Capture for 10 seconds:

```bash
sudo ./build/packet_sniffer -i <interface> -t 10 -w test.pcap
```

Capture only ICMP traffic:

```bash
sudo ./build/packet_sniffer -i <interface> -t 10 -f "icmp" -w icmp_test.pcap
```

### 2. Agent server

Start the agent:

```bash
sudo ./build/agent_server
```

The agent listens on:

- `0.0.0.0:8080`

### 3. Controller server

Start the controller:

```bash
./build/controller_server
```

The controller listens on:

- `0.0.0.0:8090`

---

## Agent HTTP API

Base URL:

```text
http://<agent-host>:8080
```

### Health

```http
GET /health
```

### Interfaces

```http
GET /interfaces
```

### Start capture

```http
POST /captures
Content-Type: application/json
```

Example request body:

```json
{
  "interface_name": "eth0",
  "filter_expression": "icmp",
  "packet_count": 0,
  "duration_seconds": 10
}
```

### List captures

```http
GET /captures
```

### Get capture by ID

```http
GET /captures/{id}
```

### Stop capture

```http
POST /captures/{id}/stop
```

---

## Controller HTTP API

Base URL:

```text
http://<controller-host>:8090
```

### List known agents

```http
GET /api/agents
```

### Add a known agent

```http
POST /api/agents
Content-Type: application/json
```

Example request body:

```json
{
  "display_name": "local-agent",
  "host": "127.0.0.1",
  "port": 8080
}
```

### Get one known agent

```http
GET /api/agents/{id}
```

### Delete one known agent

```http
DELETE /api/agents/{id}
```

### Clear all known-agent storage

```http
DELETE /api/agents
```

### Query agent health through the controller

```http
GET /api/agents/{id}/health
```

### Query agent interfaces through the controller

```http
GET /api/agents/{id}/interfaces
```

### Start remote capture through controller

```http
POST /api/agents/{id}/captures
Content-Type: application/json
```

### List remote captures through controller

```http
GET /api/agents/{id}/captures
```

### Get one remote capture through controller

```http
GET /api/agents/{id}/captures/{captureId}
```

### Stop one remote capture through controller

```http
POST /api/agents/{id}/captures/{captureId}/stop
```

---

## Example test flow

### Start the agent

```bash
sudo ./build/agent_server
```

### Start the controller

```bash
./build/controller_server
```

### Register the agent in the controller

```bash
curl -X POST http://127.0.0.1:8090/api/agents \
  -H "Content-Type: application/json" \
  -d '{
    "display_name": "local-agent",
    "host": "127.0.0.1",
    "port": 8080
  }'
```

### List agents

```bash
curl http://127.0.0.1:8090/api/agents
```

### Query agent health through the controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/health
```

### Query interfaces through the controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/interfaces
```

### Start a remote capture through the controller

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures \
  -H "Content-Type: application/json" \
  -d '{
    "interface_name": "YOUR_INTERFACE",
    "duration_seconds": 10
  }'
```

### Clear all stored agents

```bash
curl -X DELETE http://127.0.0.1:8090/api/agents
```

---

## Development direction

Planned next steps:

1. add a controller-side web interface
2. implement local discovery of agents
3. add PCAP download support
4. extend persistence beyond known-agent registry if needed

---

## Design principles

- keep the sniffer independent from HTTP concerns
- keep the agent independent from controller internals
- communicate between services using HTTP/JSON
- preserve modular boundaries so transport, storage, and capture backends can be replaced later
- build the controller as a backend-first service so a web UI can be layered on top cleanly

---

## Notes

- Packet capture typically requires elevated privileges, so `agent_server` and `packet_sniffer` may need to be run with `sudo`.
- The controller persists known agents to `data/known_agents.json` and reloads them on startup.
- Captured `.pcap` files are currently stored locally by the agent under the `captures/` directory.

---

## License

No license has been defined yet.
