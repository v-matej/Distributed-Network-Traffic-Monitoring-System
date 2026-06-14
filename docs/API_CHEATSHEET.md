# API Cheat Sheet

Quick copy-paste reference for testing the **agent** and **controller** HTTP APIs.

Assumptions used in the examples:

- agent runs on `127.0.0.1:8080`
- controller runs on `127.0.0.1:8090`
- first registered controller agent id is `agent-0001`

---

## Start the services

### Start agent

```bash
sudo ./build/agent_server
```

### Start controller

```bash
./build/controller_server
```

---

# 1. Agent API

Base URL:

```text
http://127.0.0.1:8080
```

## 1.1 Health

```bash
curl http://127.0.0.1:8080/health
```

Expected status:
- `200 OK`

---

## 1.2 Interfaces

```bash
curl http://127.0.0.1:8080/interfaces
```

Expected status:
- `200 OK`

Use one real interface name from this response in the later examples.

---

## 1.3 Start capture directly on agent

Replace `YOUR_INTERFACE` with a real interface name.

```bash
curl -X POST http://127.0.0.1:8080/captures \
  -H "Content-Type: application/json" \
  -d '{
    "interface_name": "YOUR_INTERFACE",
    "duration_seconds": 10
  }'
```

Expected status:
- `202 Accepted`

Optional fields:
- `filter_expression`
- `packet_count`
- `duration_seconds`
- `live_output`

Example with filter and packet limit:

```bash
curl -X POST http://127.0.0.1:8080/captures \
  -H "Content-Type: application/json" \
  -d '{
    "interface_name": "YOUR_INTERFACE",
    "filter_expression": "icmp",
    "packet_count": 20
  }'
```

---

## 1.4 List captures on agent

```bash
curl http://127.0.0.1:8080/captures
```

Expected status:
- `200 OK`

---

## 1.5 Get one capture from agent

Replace `CAPTURE_ID` with a real id returned from capture creation.

```bash
curl http://127.0.0.1:8080/captures/CAPTURE_ID
```

Expected status:
- `200 OK`
- `404 Not Found` if the capture id does not exist

---

## 1.6 Stop one capture on agent

```bash
curl -X POST http://127.0.0.1:8080/captures/CAPTURE_ID/stop
```

Expected status:
- `202 Accepted`
- `404 Not Found` if the capture id does not exist
- `409 Conflict` if the capture can no longer be stopped

---

# 2. Controller API

Base URL:

```text
http://127.0.0.1:8090
```

## 2.1 List known agents

```bash
curl http://127.0.0.1:8090/api/agents
```

Expected status:
- `200 OK`

---

## 2.2 Add agent to controller

```bash
curl -X POST http://127.0.0.1:8090/api/agents \
  -H "Content-Type: application/json" \
  -d '{
    "display_name": "local-agent",
    "host": "127.0.0.1",
    "port": 8080
  }'
```

Expected status:
- `201 Created`

Possible errors:
- `400 Bad Request`

---

## 2.3 Get one known agent

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001
```

Expected status:
- `200 OK`
- `404 Not Found`

---

## 2.4 Delete one known agent

```bash
curl -X DELETE http://127.0.0.1:8090/api/agents/agent-0001
```

Expected status:
- `200 OK`
- `404 Not Found`

---

## 2.5 Clear all registered agents and storage

```bash
curl -X DELETE http://127.0.0.1:8090/api/agents
```

Expected status:
- `200 OK`

---

## 2.6 Agent health through controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/health
```

Expected status:
- `200 OK`
- `404 Not Found` if the controller does not know this agent id
- `502 Bad Gateway` if the agent cannot be reached or returns an unexpected upstream error

---

## 2.7 Agent interfaces through controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/interfaces
```

Expected status:
- `200 OK`
- `404 Not Found` if the controller does not know this agent id
- `502 Bad Gateway` if the agent cannot be reached or returns an unexpected upstream error

---

## 2.8 Start remote capture through controller

Replace `YOUR_INTERFACE` with a real interface name from the selected agent.

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures \
  -H "Content-Type: application/json" \
  -d '{
    "interface_name": "YOUR_INTERFACE",
    "duration_seconds": 10
  }'
```

Expected status:
- `202 Accepted`

Possible errors:
- `400 Bad Request`
- `404 Not Found`
- `502 Bad Gateway`

Example with filter and packet limit:

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures \
  -H "Content-Type: application/json" \
  -d '{
    "interface_name": "YOUR_INTERFACE",
    "filter_expression": "icmp",
    "packet_count": 50
  }'
```

---

## 2.9 List captures through controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/captures
```

Expected status:
- `200 OK`
- `404 Not Found`
- `502 Bad Gateway`

---

## 2.10 Get one capture through controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID
```

Expected status:
- `200 OK`
- `404 Not Found`
- `502 Bad Gateway`

---

## 2.11 Stop one capture through controller

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID/stop
```

Expected status:
- `202 Accepted`
- `404 Not Found`
- `409 Conflict`
- `502 Bad Gateway`

---

## 2.12 Download one capture through controller

```bash
curl -OJ http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID/download
```

Expected status:
- `200 OK`
- `404 Not Found`
- `409 Conflict` if the capture file is not ready
- `502 Bad Gateway`

---

## 2.13 Fetch one capture to controller storage

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID/fetch
```

Expected status:
- `201 Created`
- `404 Not Found`
- `409 Conflict` if the capture file is not ready
- `502 Bad Gateway`

---

## 2.14 List controller stored captures

```bash
curl http://127.0.0.1:8090/api/controller/captures
```

Expected status:
- `200 OK`

---

## 2.15 Get one controller stored capture

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID
```

Expected status:
- `200 OK`
- `404 Not Found`

---

## 2.16 Download one controller stored capture

```bash
curl -OJ http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/download
```

Expected status:
- `200 OK`
- `404 Not Found`

---

## 2.17 Analyze one controller stored capture

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/analysis
```

Expected status:
- `200 OK`
- `404 Not Found`

Notes:
- Analysis is cached in `data/captures/<agent_id>/<capture_id>.analysis.json`.
- Cache is reused only when PCAP file size and modification time match.

---

## 2.18 List packets from one controller stored capture

```bash
curl "http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/packets?offset=0&limit=50"
```

Expected status:
- `200 OK`
- `404 Not Found`

---

## 2.19 Inspect one packet from a controller stored capture

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/packets/1
```

Expected status:
- `200 OK`
- `404 Not Found`

---

## 2.20 Delete one controller stored capture

```bash
curl -X DELETE http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID
```

Expected status:
- `200 OK`
- `404 Not Found`

Notes:
- Deletion removes the controller-side `.pcap`, `.json`, and `.analysis.json` files for that capture.

---

# 3. Suggested test flow

## 3.1 Start both services

```bash
sudo ./build/agent_server
```

```bash
./build/controller_server
```

## 3.2 Register the agent in the controller

```bash
curl -X POST http://127.0.0.1:8090/api/agents \
  -H "Content-Type: application/json" \
  -d '{
    "display_name": "local-agent",
    "host": "127.0.0.1",
    "port": 8080
  }'
```

## 3.3 Get interfaces through controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/interfaces
```

## 3.4 Start a capture through controller

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures \
  -H "Content-Type: application/json" \
  -d '{
    "interface_name": "YOUR_INTERFACE",
    "duration_seconds": 10
  }'
```

## 3.5 List captures through controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/captures
```

## 3.6 Get one capture through controller

```bash
curl http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID
```

## 3.7 Stop one long-running capture through controller

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID/stop
```

## 3.8 Fetch completed capture to controller storage

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID/fetch
```

## 3.9 Analyze the stored capture

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/analysis
```

## 3.10 Inspect packets from the stored capture

```bash
curl "http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/packets?offset=0&limit=20"
```

## 3.11 Restart controller and confirm persistence

```bash
pkill controller_server
./build/controller_server
curl http://127.0.0.1:8090/api/agents
```

## 3.12 Clear all agents and persisted storage

```bash
curl -X DELETE http://127.0.0.1:8090/api/agents
```

---

# 4. Useful notes

- `agent_server` usually needs `sudo` because packet capture typically requires elevated privileges.
- The controller persists known agents to `data/known_agents.json`.
- The controller stores fetched captures under `data/captures/<agent_id>/`.
- `DELETE /api/agents` clears registered agents and controller capture storage.
- Stored captures can be downloaded, analyzed, inspected packet by packet, or deleted through the controller API.
- For early manual testing, `curl` is enough. Swagger/OpenAPI can be added later once the API stabilizes further.
