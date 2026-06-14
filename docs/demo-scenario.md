# Final Demo Scenario

This walkthrough is designed for the final diploma demonstration of the Distributed Network Traffic Monitoring System v1.0.0.

The goal is to show that one controller can manage multiple agents, run remote captures, persist completed PCAP files on controller storage, analyze stored captures, inspect individual packets, and keep stored captures usable even when an agent is offline.

---

## 1. Demo Topology

Use three Linux virtual machines on the same host-only or internal network:

```text
Network: 192.168.56.0/24

controller-vm   192.168.56.10   controller_server + frontend UI
agent-1-vm      192.168.56.11   agent_server
agent-2-vm      192.168.56.12   agent_server
```

Recommended VirtualBox adapters:

- Adapter 1: NAT, used for package installation
- Adapter 2: Host-only Adapter, used for the demo network

Before starting the demo, confirm basic connectivity:

```bash
ping -c 3 192.168.56.10
ping -c 3 192.168.56.11
ping -c 3 192.168.56.12
```

Run these checks from the machines that need to communicate with each other.

---

## 2. Prepare the Controller VM

On `controller-vm`, build the backend and frontend:

```bash
cd Distributed-Network-Traffic-Monitoring-System
make backend-build
make frontend-install
make frontend-build
```

Start the controller server in one terminal:

```bash
cd Distributed-Network-Traffic-Monitoring-System/backend
./build/controller_server
```

Expected result:

```text
Controller HTTP server listening on 0.0.0.0:8090
```

Start the frontend UI in a second terminal:

```bash
cd Distributed-Network-Traffic-Monitoring-System
make frontend-dev
```

Open the UI from the host machine or controller VM:

```text
http://192.168.56.10:5173
```

If running locally on the controller VM, this also works:

```text
http://127.0.0.1:5173
```

---

## 3. Prepare Agent 1

On `agent-1-vm`, build and start the agent:

```bash
cd Distributed-Network-Traffic-Monitoring-System
make backend-build
cd backend
sudo ./build/agent_server
```

Expected result:

```text
Agent HTTP server listening on 0.0.0.0:8080
```

Packet capture requires elevated privileges, so the agent should be started with `sudo`.

---

## 4. Prepare Agent 2

On `agent-2-vm`, build and start the agent:

```bash
cd Distributed-Network-Traffic-Monitoring-System
make backend-build
cd backend
sudo ./build/agent_server
```

Expected result:

```text
Agent HTTP server listening on 0.0.0.0:8080
```

---

## 5. Manually Add Agents

In the controller UI:

1. Open `Agents`.
2. Add Agent 1:

```text
Display name: Agent 1
Host: 192.168.56.11
Port: 8080
```

3. Add Agent 2:

```text
Display name: Agent 2
Host: 192.168.56.12
Port: 8080
```

4. Confirm both agents appear in the table.
5. Confirm their health status becomes available.

Optional API equivalent:

```bash
curl -X POST http://127.0.0.1:8090/api/agents \
  -H "Content-Type: application/json" \
  -d '{
    "display_name": "Agent 1",
    "host": "192.168.56.11",
    "port": 8080
  }'
```

```bash
curl -X POST http://127.0.0.1:8090/api/agents \
  -H "Content-Type: application/json" \
  -d '{
    "display_name": "Agent 2",
    "host": "192.168.56.12",
    "port": 8080
  }'
```

Talking point:

```text
The controller stores known agents in backend/data/known_agents.json, so registered agents survive a controller restart.
```

---

## 6. Show Agent Detail

In the UI:

1. Open `Agents`.
2. Click `Agent 1`.
3. Point out:
   - health panel
   - metadata panel
   - capture target interface selector
   - capture filter builder
   - remote captures table
   - controller stored captures table

Choose the interface connected to `192.168.56.0/24`. Common names are:

```text
enp0s8
eth1
ens33
```

Use the interface list shown by the UI as the source of truth.

---

## 7. Demo Capture 1: ICMP Traffic

In the Agent 1 detail page:

1. Select the host-only interface.
2. Set a duration of `30` seconds.
3. Use a filter:

```text
icmp
```

4. Start the capture.

While the capture is running, generate ICMP traffic from `controller-vm`:

```bash
ping -c 10 192.168.56.11
```

In the UI:

1. Watch the capture appear as active.
2. Wait for it to complete.
3. Open the capture detail page.

Talking point:

```text
The controller starts the capture remotely, but the actual packet capture runs on the selected agent.
```

---

## 8. Fetch Capture to Controller

On the completed capture detail page:

1. Click `Fetch to controller`.
2. Confirm the stored capture appears in the controller stored captures area.
3. Open Agent Detail and confirm the same capture is listed under controller stored captures.

Optional API equivalent:

```bash
curl -X POST http://127.0.0.1:8090/api/agents/agent-0001/captures/CAPTURE_ID/fetch
```

On `controller-vm`, confirm the files exist:

```bash
cd Distributed-Network-Traffic-Monitoring-System/backend
find data/captures -maxdepth 3 -type f | sort
```

Expected stored files:

```text
data/captures/<agent_id>/<capture_id>.pcap
data/captures/<agent_id>/<capture_id>.json
```

Talking point:

```text
Fetching a capture copies the PCAP from the agent to controller storage. This makes analysis and packet inspection possible even if the agent later goes offline.
```

---

## 9. Demo Analysis View

Open the fetched capture detail page and use the analysis section.

Show:

- total packets
- total bytes
- capture duration
- average packet size
- main protocol
- main service
- external traffic detected
- DNS traffic detected
- protocol distribution bars
- top conversations
- top destination services
- top destination IPs with classification
- advanced counters

Optional API equivalent:

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/analysis
```

After analysis runs once, confirm the cache file exists:

```bash
cd Distributed-Network-Traffic-Monitoring-System/backend
find data/captures -name "*.analysis.json" -print
```

Talking point:

```text
The controller caches analysis results in <capture_id>.analysis.json. On later page loads, analysis is returned from cache if the PCAP size and modification time have not changed.
```

---

## 10. Demo Packet Inspection

From the stored capture table:

1. Click `Packet view`.
2. Show the packet list.
3. Select one packet.
4. Show:
   - decoded layer fields
   - packet metadata
   - hex output

Optional API equivalents:

```bash
curl "http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/packets?offset=0&limit=20"
```

```bash
curl http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID/packets/1
```

Talking point:

```text
Packet inspection uses the controller-stored PCAP file, not the live agent.
```

---

## 11. Demo Capture 2: HTTP Service Traffic

Use Agent 2 to show a second agent and a different traffic pattern.

In the Agent 2 detail page:

1. Select the host-only interface.
2. Set a duration of `30` seconds.
3. Use this filter:

```text
tcp port 8080
```

4. Start the capture.

From `controller-vm`, generate HTTP traffic against Agent 2:

```bash
curl http://192.168.56.12:8080/health
curl http://192.168.56.12:8080/interfaces
```

Repeat the commands a few times while the capture is active.

After the capture completes:

1. Open the capture.
2. Fetch it to the controller.
3. Run analysis.
4. Point out the top destination service for `tcp/8080` as HTTP alternate.

Talking point:

```text
The service mapping makes the analysis easier to explain than raw port counters.
```

---

## 12. Offline-Safe Stored Capture Demo

Stop Agent 1:

```text
Press Ctrl+C in the agent-1-vm agent_server terminal.
```

In the UI:

1. Open Agent 1 detail.
2. Refresh.
3. Point out that health is unavailable.
4. Point out that controller stored captures are still visible.
5. Open a stored capture.
6. Open analysis.
7. Open packet view.

Talking point:

```text
The controller can still render stored captures because the PCAP, metadata, and analysis cache live on controller storage.
```

Restart Agent 1 before continuing if more live capture steps are needed:

```bash
cd Distributed-Network-Traffic-Monitoring-System/backend
sudo ./build/agent_server
```

---

## 13. Stored Capture Deletion

In the UI:

1. Open Agent Detail or Captures.
2. Find a controller stored capture.
3. Click `Delete`.
4. Confirm it disappears from the stored capture list.

Optional API equivalent:

```bash
curl -X DELETE http://127.0.0.1:8090/api/controller/captures/agent-0001/CAPTURE_ID
```

Confirm sidecar files were removed:

```bash
cd Distributed-Network-Traffic-Monitoring-System/backend
find data/captures -maxdepth 3 -type f | sort
```

Talking point:

```text
Deleting a stored capture removes the controller-side PCAP, metadata JSON, and cached analysis JSON for that capture.
```

---

## 14. Suggested Presentation Order

1. Show topology diagram or explain the three-VM setup.
2. Start both agents.
3. Start controller server.
4. Start frontend UI.
5. Add both agents manually.
6. Show dashboard and agent health.
7. Run ICMP capture on Agent 1.
8. Fetch Agent 1 capture to controller.
9. Show analysis and packet inspection.
10. Run HTTP capture on Agent 2.
11. Fetch and analyze Agent 2 capture.
12. Stop Agent 1 and show offline-safe stored capture access.
13. Show cached analysis file.
14. Delete one stored capture.
15. End on the design summary.

---

## 15. Key Points to Explain

- The architecture separates capture execution from centralized management.
- Agents perform packet capture close to the traffic source.
- The controller coordinates agents over HTTP/JSON.
- The UI is a practical operator interface, not only a testing tool.
- Fetch-to-controller storage protects captures from agent unavailability.
- Analysis output is designed for explanation, not only raw counters.
- Cached analysis avoids re-running PCAP analysis every time the page is opened.
- Packet inspection allows low-level verification of the analysis.

---

## 16. Reset Between Demo Runs

To clear all registered agents and stored captures from the UI:

1. Open `Settings`.
2. Use the clear storage action.

API equivalent:

```bash
curl -X DELETE http://127.0.0.1:8090/api/agents
```

This clears the registered agents and controller capture storage.

To remove only build output:

```bash
make clean
```
